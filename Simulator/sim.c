// -------------------------------------------------------------------------Tomasulo Project-------------------------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


// -------------------------------------------------------------------------- Definitions ----------------------------------------------------------------------------------
#define NUM_OF_REGS 16
#define MAX_MEMIN_LINES 4096
#define INST_QUEUE_SIZE 16
#define TAG_SIZE 6
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

//----------------------------------------------------------------------------- Struct -------------------------------------------------------------------------------------

typedef struct instruction { // contains all basic fields of an instruction with extra fields for trace
    int opcode; // 2 - ADD, 3 - SUB, 4 - MUL, 5 - DIV, 6 - HALT
    int dst;
    int src0;
    int src1;
    int pc;
    float result;
    // info for trace:
    char tag[TAG_SIZE];     //should be printed in text form: "ADD0", "MUL1" etc....
    int raw_inst;           // 8-digit number of full instruction. a copy of memin_line.
    int clk_fetched;        // will be -1 by default. determining the clock cycle the instruction was fetched
    int clk_issued;         // will be -1 by default. determining the clock cycle the instruction was issued
    int clk_start;          // will be -1 by default. determining the clock cycle the instruction started execution
    int clk_end;            // will be -1 by default. determining the clock cycle the instruction will end execution
    int clk_write;          // will be -1 by default. determining the clock cycle the instruction will be written to CDB
} inst; 

typedef struct reservation_station{     // reservation station table 'lines'
    char name[TAG_SIZE]; 
    int pc; // Added pc in order to help choosing which coomand to execute first if there are two ready ones
    bool busy; // default to false
    int opcode;
    float Vj;
    float Vk;
    char Qj[TAG_SIZE];
    char Qk[TAG_SIZE];
    int num_func_unit;
    int index;
}res_station;

typedef struct functional_unit {        // the number of resservation statiosn and functional units can be different
    bool busy;                          // default is false
    int delay;                          // value coming from cfg.txt
} func_unit;

typedef struct instruction_queue {      // instructions queue - will enqueue instructions on fetch and dequeue when issue.
    inst * inst_q[INST_QUEUE_SIZE];
    int front;
    int rear;
}inst_queue;

typedef struct reg {                   // register status "line"
    float value;
    int valid;
    char tag[TAG_SIZE];
}reg;

typedef struct CDB_station {
    bool busy;
    float data;
    char tag[TAG_SIZE]; 
    int dest;
    int cdb_type; // 2 - ADD, 4 - MUL, 5 - DIV. NO OTHER OPTIONS!
} cdb;

typedef struct trace_CDB {
    int cycle;
    int pc;
    int cdb_type;  // called "cdb_name" in project. we have exactly 3 options: 2-"ADD", 4-"MUL", 5-"DIV".
    float data;
    char tag[TAG_SIZE];
} trace_CDB;

//--------------------------------------------------------------------- Global variables & Declerations ---------------------------------------------------------------------------------------------

// values to the following vars will be assigned in initialization stage, using cfg.txt input file:
int add_nr_units, mul_nr_units, div_nr_units, add_nr_reservation, mul_nr_reservation, div_nr_reservation, add_delay, mul_delay, div_delay;

reg regs[NUM_OF_REGS];                      // regs_status array
inst inst_arr[MAX_MEMIN_LINES];             // instructions array
int total_inst_count; 
inst_queue instruction_queue;       
trace_CDB trace_cdb[MAX_MEMIN_LINES];       // A list of cdb traces to be printed at the end of execution
int trace_cdb_count;                        // number of items in trace_cdb to print

int clock = 0;
int halt = 0;
int done = 0;
int inst_pc = 0;  // for fetch

func_unit* add_units;
func_unit* mul_units;
func_unit* div_units;

res_station* add_reservation;
res_station* mul_reservation;
res_station* div_reservation;

cdb add_cdb;
cdb mul_cdb;
cdb div_cdb;

// variables to keep track of number of free functional units 
int free_add_units;
int free_mul_units;
int free_div_units;

// file handing functions:
FILE* safe_file_open(const char* name, const char* mode);
void config(char* cfg);
void memin_read(char* memin);
void print_regout(char* regout);
void print_trace_inst(char* trace_inst);
void print_trace_cdb(char* trace_cdb_f);

// pipe build functions:
void build_functional_units();
void build_reservation_station();
void init_cdb();

// general functions:
void create_regs();
inst create_inst(char *memin_line, int i);
int find_res_station(res_station* reservation_station, int station_num);
int into_res_staion(inst* instruction);
void initial_queue();
bool queue_is_full();
bool queue_is_empty();
int enqueue_inst_q(inst* instruction, int inst_pc);
inst* dequeue();
bool is_ready_for_execution(res_station* station);
int compare_pc(const void *a, const void *b);
void execute_inst(res_station* ready_to_execute, int num_ready , int * free_units);
int find_free_unit(int opcode);
void clear_station(int opcode, int station);
void cdb_write(int type, int pc_selected);
void is_done();

// Pipe function:
int fetch();
void issue();
void execute();
void write_result();

//--------------------------------------------------------------------- File handling -----------------------------------------------------------------------------------------------

// Opens an input_memory_file with error handling.
FILE* safe_file_open(const char* name, const char* mode) {
    FILE* input_memory_file = fopen(name, mode);
    if (input_memory_file == NULL) {
        printf("Error: Could not open file: '%s' with mode: '%s'\n", name, mode);
        exit(1);
    }
    return input_memory_file;
}

void config(char* cfg) {  // getting the configuration file name , parsing it and assigning values to variables
    FILE* config_file = safe_file_open(cfg, "r");
    char cfg_line[100];
    
    while (fgets(cfg_line, sizeof(cfg_line), config_file) != NULL) {
        char param[20]; 
        char value[10]; 
        // Tokenize the line (only 2 'words')
        char *token = strtok(cfg_line, " =\n");
        strcpy(param, token);
        token = strtok(NULL, " =\n");
        strcpy(value, token);
        if (strcmp(param, "add_nr_units") == 0) {
            add_nr_units = atoi(value);
        }
        else if (strcmp(param, "mul_nr_units") == 0) {
            mul_nr_units = atoi(value);
        }
        else if (strcmp(param, "div_nr_units") == 0) {
            div_nr_units = atoi(value);
        }
        else if (strcmp(param, "add_nr_reservation") == 0) {
            add_nr_reservation = atoi(value);
        }
        else if (strcmp(param, "mul_nr_reservation") == 0) {
            mul_nr_reservation = atoi(value);
        }
        else if (strcmp(param, "div_nr_reservation") == 0) {
            div_nr_reservation = atoi(value);
        }
        else if (strcmp(param, "add_delay") == 0) {
            add_delay = atoi(value);
        }
        else if (strcmp(param, "mul_delay") == 0) {
            mul_delay = atoi(value);
        }
        else if (strcmp(param, "div_delay") == 0) {
            div_delay = atoi(value);
        }
    } 
    fclose(config_file);
}
void memin_read(char* memin){
    FILE* memin_file = safe_file_open(memin, "r");
    char memin_line[20]; // line is 8 nums + 1 '\n' + 1 '\0' + room for extra spaces
    int i = 0;
    inst instruction;
    total_inst_count = 0;
    while (fgets(memin_line, sizeof(memin_line), memin_file) != NULL) {
        if (strlen(memin_line)>8 && memin_line[8] == '\n') // removing '\n' at the end of the line
        {
            memin_line[8] = '\0';
        }
        if (atoi(memin_line) !=0) // check if this condition is ok or we prefer create instructions to zeros in memin.txt (and just remove this condition)
        {
            instruction = create_inst(memin_line, i);
            inst_arr[i] = instruction;  
            i++;
            total_inst_count++;
        } else break;
    } 
    fclose(memin_file);
}

void print_regout(char* regout){
    FILE* regout_file = safe_file_open(regout, "w");
    int i = 0;
    while (i<NUM_OF_REGS)
    {
        fprintf(regout_file,"%.06f\n",regs[i].value);
        i++;
    }
    fclose(regout_file);
}

void print_trace_inst(char* trace_inst){
    FILE* trace_inst_file = safe_file_open(trace_inst, "w");
    int i = 0;
    while (i<total_inst_count) // iterating over all instructions
    {
        if (inst_arr[i].opcode !=6){
            fprintf(trace_inst_file,"0%x %d %s %d %d %d %d\n",
                inst_arr[i].raw_inst, inst_arr[i].pc, inst_arr[i].tag, inst_arr[i].clk_issued, inst_arr[i].clk_start, inst_arr[i].clk_end, inst_arr[i].clk_write); // print types are WRONG. needs fixing after creating instructions!
                // test prints:
                //fprintf(trace_inst_file,"%d %.0f %.0f %.0f\n",inst_arr[i].opcode,inst_arr[i].dst,inst_arr[i].src0,inst_arr[i].src1);

            i++;
        } else break;
    }
    fclose(trace_inst_file);
}

void print_trace_cdb(char* trace_cdb_f){
    FILE* trace_cdb_file = safe_file_open(trace_cdb_f, "w");
    int i = 0;
    char type[4]; // Array for type
    while (i<trace_cdb_count) // iterating over all cdb traces
    {   
        //change cdb_type to text before printing:
        switch (trace_cdb[i].cdb_type)
        {
        case 2: // ADD
            strcpy(type, "ADD");
            break;
        case 4: // MUL
            strcpy(type, "MUL");
            break;
        case 5: // DIV
            strcpy(type, "DIV");
            break;
        default:
            printf("wrong cdb type found in trace! printing 'NAN' to file\n");
            strcpy(type, "NAN");
            break;
        }
        // printing line of trace to file:
        fprintf(trace_cdb_file,"%d %d %s %.06f %s\n",
            trace_cdb[i].cycle, trace_cdb[i].pc, type, trace_cdb[i].data, trace_cdb[i].tag); 
        i++;
    }
    
    fclose(trace_cdb_file);
    return;
}

// ------------------------------------------------------------- Pipe Build --------------------------------------------------------------------------

void build_functional_units() {
    for (int i = 0; i < add_nr_units; i++) {
        func_unit add_unit;
        add_unit.delay = add_delay;
        add_unit.busy = false;
        add_units[i] = add_unit;
    }
    for (int i = 0; i < mul_nr_units; i++) {
        func_unit mul_unit;
        mul_unit.delay = mul_delay;
        mul_unit.busy = false;
        mul_units[i] = mul_unit;
    }
    for (int i = 0; i < div_nr_units; i++) {
        func_unit div_unit;
        div_unit.delay = div_delay;
        div_unit.busy = false;
        div_units[i] = div_unit;
    }
    return;
}

void build_reservation_station() {
    for (int i = 0; i < add_nr_reservation; i++){
        res_station add_line;
        add_line.busy = false;
        add_line.pc=-1;
        add_line.opcode=-1;
        add_line.Vj=NAN;         // Initialize Vj and Vk with NAN - not a number
        add_line.Vk=NAN;
        strcpy(add_line.Qj, ""); // Initialize Qj and Qk with an empty string
        strcpy(add_line.Qk, "");
        sprintf(add_line.name, "ADD%d", i);
        add_line.num_func_unit=-1;
        add_line.index=i;
        add_reservation[i] = add_line; 
    }
    for (int i = 0; i < mul_nr_reservation; i++) {
        res_station mul_line;
        mul_line.busy = false;
        mul_line.pc=-1;
        mul_line.opcode=-1;
        mul_line.Vj=NAN;
        mul_line.Vk=NAN;
        strcpy(mul_line.Qj, ""); 
        strcpy(mul_line.Qk, "");
        sprintf(mul_line.name, "MUL%d", i);
        mul_line.num_func_unit=-1;
        mul_line.index=i;
        mul_reservation[i] = mul_line;
    }
    for (int i = 0; i < div_nr_reservation; i++) {
        res_station div_line;
        div_line.busy = false;
        div_line.pc=-1;
        div_line.opcode=-1;
        div_line.Vj=NAN;
        div_line.Vk=NAN;
        strcpy(div_line.Qj, "");
        strcpy(div_line.Qk, "");
        sprintf(div_line.name, "DIV%d", i);
        div_line.num_func_unit=-1;
        div_line.index=i;
        div_reservation[i] = div_line;
    }
    return;
}

void init_cdb(){
    // add_cdb init:
    add_cdb.data = NAN; // NotANumber
    add_cdb.dest = -1;
    strcpy(add_cdb.tag, "");  // Initialize tag with an empty string
    add_cdb.cdb_type = 2;     // 2 - ADD, 4 - MUL, 5 - DIV. NO OTHER OPTIONS!

    // mul_cdb init:
    mul_cdb.data = NAN; 
    mul_cdb.dest = -1;
    strcpy(mul_cdb.tag, "");  
    mul_cdb.cdb_type = 4; 

    // div_cdb init:
    div_cdb.data = NAN; 
    div_cdb.dest = -1;
    strcpy(div_cdb.tag, "");  
    div_cdb.cdb_type = 5; 

    // init trace_cdb:
    trace_cdb_count = 0; // everytime we print to cdb, we will add to the trace_cdb and its counter

    return;
}


// ------------------------------------------------------------------ General functions --------------------------------------------------------------------
void create_regs() {
    // initialize all 16 registers to requested initial values
    for (int i = 0; i < NUM_OF_REGS; i++) {
        reg regist;
        regist.valid = 1;
        regist.value = (float)i;
        regs[i] = regist;
    }
}

inst create_inst(char *memin_line, int i){
    inst instruction;
    // Convert HEX string to integer
    int memin_int = strtol(memin_line, NULL, 16);
    // fill instruction fields from memin_line    
    // Extracting opcode, destination, source0, and source1
    // 0000-OPCD-DEST-SRC0-SRC1-0000-0000-0000
    instruction.opcode = (memin_int >> 24) & 0xF;
    instruction.dst = ((memin_int >> 20) & 0xF);
    instruction.src0 = ((memin_int >> 16) & 0xF); 
    instruction.src1 = ((memin_int >> 12) & 0xF);
    // set default values
    instruction.pc = i; // i is the munber of the line in memin.txt, therefore it's the pc for the instruction
    instruction.result = NAN; // NotANumber
    instruction.raw_inst = memin_int;
    instruction.clk_fetched = -1;
    instruction.clk_issued = -1;
    instruction.clk_start = -1;
    instruction.clk_end = -1;
    instruction.clk_write = -1;
    strcpy(instruction.tag, ""); //initialize the instruction tag to empty string
    return instruction;
}

int find_res_station(res_station* reservation_station, int station_num) {
    for (int i = 0; i < station_num; i++)
    {
        if (!reservation_station[i].busy) return i;
    }
    return(-1);     // all reservation stations are busy
}

int into_res_staion(inst * instruction) {
    int station=-1;
    switch (instruction->opcode)
    {
    case 2:     // ADD/SUB cases are the same
    case 3:
        station = find_res_station(add_reservation, add_nr_reservation);
        if (station == -1) return -1;
        add_reservation[station].busy = true;
        add_reservation[station].opcode=instruction->opcode;
        add_reservation[station].pc=instruction->pc;
        if (!regs[instruction->src0].valid && isnan(add_reservation[station].Vj)) {
            strcpy(add_reservation[station].Qj , regs[instruction->src0].tag);
        } else {
            add_reservation[station].Vj = regs[instruction->src0].value;
        }
        if (!regs[instruction->src1].valid && isnan(add_reservation[station].Vk)) {
            strcpy(add_reservation[station].Qk , regs[instruction->src1].tag);
        } else {
            add_reservation[station].Vk = regs[instruction->src1].value;
        }
        break;
    case 4:
        station = find_res_station(mul_reservation, mul_nr_reservation);
        if (station == -1) return -1;
        mul_reservation[station].busy = true;
        mul_reservation[station].opcode=instruction->opcode;
        mul_reservation[station].pc=instruction->pc;
        if (!regs[instruction->src0].valid && isnan(mul_reservation[station].Vj)) {
            strcpy(mul_reservation[station].Qj , regs[instruction->src0].tag);
        } else {
            mul_reservation[station].Vj = regs[instruction->src0].value;
        }
        if (!regs[instruction->src1].valid && isnan(mul_reservation[station].Vk)) {
            strcpy(mul_reservation[station].Qk , regs[instruction->src1].tag);
        } else {
            mul_reservation[station].Vk = regs[instruction->src1].value;
        }
        break;
    case 5:
        station = find_res_station(div_reservation, mul_nr_reservation);
        if (station == -1) return -1;
        div_reservation[station].busy = true;
        div_reservation[station].opcode=instruction->opcode;
        div_reservation[station].pc=instruction->pc;
        if (!regs[instruction->src0].valid && isnan(div_reservation[station].Vj)) {
            strcpy(div_reservation[station].Qj , regs[instruction->src0].tag);
        } else {
            div_reservation[station].Vj = regs[instruction->src0].value;
        }
        if (!regs[instruction->src1].valid && isnan(div_reservation[station].Vk)) {
            strcpy(div_reservation[station].Qk , regs[instruction->src1].tag);
        } else {
            div_reservation[station].Vk = regs[instruction->src1].value;
        }
        break;
    case 6:
        // halt - do nothing
        break;
    default:
        printf("ERROR: instruction.opcode invalid ! the opcode: %d", instruction->opcode);
        return 0;
        break;
    }
    return station;
}

void initial_queue() {
    instruction_queue.front = -1;
    instruction_queue.rear = -1;
}

bool queue_is_full() {
    return ((instruction_queue.rear + 1) % INST_QUEUE_SIZE == instruction_queue.front);
}

bool queue_is_empty() {
    return (instruction_queue.front == -1 && instruction_queue.rear == -1);
}

int enqueue_inst_q(inst * instruction, int inst_pc) {  // return 0 if cant enqueue, 1 if succeed
    if (queue_is_full()) {
        //FIFO is full. Cannot enqueue
        return 0;
    }
    if (queue_is_empty()) {
        instruction_queue.front = 0;
        instruction_queue.rear = 0;
    } else {
        instruction_queue.rear = (instruction_queue.rear + 1) % INST_QUEUE_SIZE;
    }
    instruction_queue.inst_q[instruction_queue.rear] = instruction; 
    instruction_queue.inst_q[instruction_queue.rear]->clk_fetched = clock;
    return 1;
}

inst* dequeue() {
    inst * instruction= NULL;
    if (queue_is_empty()) {
        // Queue is empty. Cannot dequeue
        return instruction; // error
    }
    instruction = instruction_queue.inst_q[instruction_queue.front];
    if (instruction_queue.front == instruction_queue.rear) {  // queue has only one element, reset front and rear
        instruction_queue.front = -1;
        instruction_queue.rear = -1;
    }
    else {
        instruction_queue.front = (instruction_queue.front + 1) % INST_QUEUE_SIZE;
    }
    return instruction;
}

bool is_ready_for_execution(res_station* station) {
    bool ready =0; 
    if (!isnan(station->Vj) && !isnan(station->Vk) && (station->num_func_unit == -1) && (inst_arr[station->pc].clk_issued !=clock)) {
        ready=1;
    }
    return ready;
}

int compare_pc(const void *a, const void *b) {
    const res_station *res_station_a = *(const res_station **)a;
    const res_station *res_station_b = *(const res_station **)b;
    if (res_station_a->pc < res_station_b->pc) {
        return -1;
    } else if (res_station_a->pc > res_station_b->pc) {
        return 1;
    } else {
        return 0;
    }
}

int find_free_unit(int opcode) {
    switch (opcode)
    {
    case 2: //add
    case 3: //sub
        for (int i = 0; i < add_nr_units; i++) {
            if (!add_units[i].busy) return i;
        }
        // reaching here means all ADD units are busy
        return (-1);
        break;
    case 4: //mul
        for (int i = 0; i < mul_nr_units; i++) {
            if(!mul_units[i].busy) return i;
        }
        // reaching here means all MUL units are busy
        return (-1);
        break;
    case 5: //div
        for (int i = 0; i < div_nr_units; i++) {
            if (!div_units[i].busy) return i;
        }
        // reaching here means all DIV units are busy
        return (-1);
        break;
    default:
        printf("invalid opcode! the opcode %d shouldnt be in res_station\n", opcode);
        return (-1);
        break;
    }
}

void execute_inst(res_station* ready_to_execute, int num_ready , int * free_units) {
    int units_to_use = MIN(num_ready, *free_units);
    for (int i = 0; i < units_to_use; i++) {
        int unit = find_free_unit(ready_to_execute[i].opcode);
        if (unit == -1) {
            // unit = -1 , shouldnt execute
            break;
        }
        // reaching here means we found free func unit
        ready_to_execute[i].num_func_unit =unit;
        inst_arr[ready_to_execute[i].pc].clk_start = clock;
        switch (ready_to_execute[i].opcode) {
            case 2: // add
                //Executing ADD inst
                add_units[unit].busy = true;
                inst_arr[ready_to_execute[i].pc].clk_end = clock+add_delay-1;
                inst_arr[ready_to_execute[i].pc].result = ready_to_execute[i].Vj + ready_to_execute[i].Vk;
                add_reservation[ready_to_execute[i].index].num_func_unit=unit;
                break;
            case 3: // sub
                add_units[unit].busy = true;
                //Executing SUB inst
                inst_arr[ready_to_execute[i].pc].clk_end = clock+add_delay-1;
                inst_arr[ready_to_execute[i].pc].result = ready_to_execute[i].Vj - ready_to_execute[i].Vk;
                add_reservation[ready_to_execute[i].index].num_func_unit=unit;
                break;
            case 4: // mul
                mul_units[unit].busy = true;
                //Executing MUL inst
                inst_arr[ready_to_execute[i].pc].clk_end = clock+mul_delay-1;
                inst_arr[ready_to_execute[i].pc].result = ready_to_execute[i].Vj * ready_to_execute[i].Vk;
                mul_reservation[ready_to_execute[i].index].num_func_unit=unit;
                break;
            case 5: // div
                div_units[unit].busy = true;
                //Executing DIV inst
                inst_arr[ready_to_execute[i].pc].clk_end = clock+div_delay-1;
                div_reservation[ready_to_execute[i].index].num_func_unit=unit;
                if (ready_to_execute[i].Vk != 0) {
                    inst_arr[ready_to_execute[i].pc].result = ready_to_execute[i].Vj / ready_to_execute[i].Vk;
                } else {
                    printf ("Division by 0 is wrong..\n"); //Decide what to do here
                }
                break;   
            default:
                printf("Unknown opcode: %d\n",ready_to_execute[i].opcode);
                break;
        }
        // Decrement the number of free functional units
        (*free_units)--;
    }
}

void clear_station(int opcode, int station) {
    switch (opcode)
    {
    case 2:
        add_reservation[station].busy = false;
        add_reservation[station].pc = -1;
        add_reservation[station].opcode = -1;
        add_reservation[station].Vj = NAN;
        add_reservation[station].Vk = NAN;
        strcpy(add_reservation[station].Qj, ""); // Initialize Qj and Qk with an empty string
        strcpy(add_reservation[station].Qk, "");
        add_reservation[station].num_func_unit = -1;
        break;
    case 4:
        mul_reservation[station].busy = false;
        mul_reservation[station].pc = -1;
        mul_reservation[station].opcode = -1;
        mul_reservation[station].Vj = NAN;
        mul_reservation[station].Vk = NAN;
        strcpy(mul_reservation[station].Qj, ""); // Initialize Qj and Qk with an empty string
        strcpy(mul_reservation[station].Qk, "");
        mul_reservation[station].num_func_unit = -1;
        break;
    case 5:
        div_reservation[station].busy = false;
        div_reservation[station].pc = -1;
        div_reservation[station].opcode = -1;
        div_reservation[station].Vj = NAN;
        div_reservation[station].Vk = NAN;
        strcpy(div_reservation[station].Qj, ""); // Initialize Qj and Qk with an empty string
        strcpy(div_reservation[station].Qk, "");
        div_reservation[station].num_func_unit = -1;
        break;
    default:
        printf("clear_station: invalid opcode %d , can only get 3,4,5", opcode); // ramove?
        break;
    }
}

void cdb_write(int type, int pc_selected) {
    switch (type)
    {
    case 2:
        if (strcmp(regs[add_cdb.dest].tag , add_cdb.tag) == 0) {
            regs[add_cdb.dest].valid = 1;
            regs[add_cdb.dest].value = add_cdb.data;
            strcpy(regs[add_cdb.dest].tag, "");
        }
        trace_cdb[trace_cdb_count].cycle = clock;
        trace_cdb[trace_cdb_count].pc = pc_selected;
        trace_cdb[trace_cdb_count].cdb_type = 2;
        trace_cdb[trace_cdb_count].data = add_cdb.data;
        strcpy(trace_cdb[trace_cdb_count].tag, add_cdb.tag);
        trace_cdb_count += 1;
        for (int i = 0; i < add_nr_reservation; i++)
        {
            if (add_reservation[i].busy && add_reservation[i].num_func_unit == -1) {
                if (strcmp(add_reservation[i].Qj ,add_cdb.tag) == 0) {add_reservation[i].Vj = add_cdb.data; strcpy(add_reservation[i].Qj, "");}
                if (strcmp(add_reservation[i].Qk ,add_cdb.tag) == 0) {add_reservation[i].Vk = add_cdb.data; strcpy(add_reservation[i].Qk, "");}
            }
        }
        for (int i = 0; i < mul_nr_reservation; i++)
        {
            if (mul_reservation[i].busy && mul_reservation[i].num_func_unit == -1) {
                if (strcmp(mul_reservation[i].Qj ,add_cdb.tag) == 0) {mul_reservation[i].Vj = add_cdb.data; strcpy(mul_reservation[i].Qj, "");}
                if (strcmp(mul_reservation[i].Qk ,add_cdb.tag) == 0) {mul_reservation[i].Vk = add_cdb.data; strcpy(mul_reservation[i].Qk, "");}
            }
        }
        for (int i = 0; i < div_nr_reservation; i++)
        {
            if (div_reservation[i].busy && div_reservation[i].num_func_unit == -1) {
                if (strcmp(div_reservation[i].Qj ,add_cdb.tag) == 0) {div_reservation[i].Vj = add_cdb.data; strcpy(div_reservation[i].Qj, "");}
                if (strcmp(div_reservation[i].Qk ,add_cdb.tag) == 0) {div_reservation[i].Vk = add_cdb.data; strcpy(div_reservation[i].Qk, "");}
            }
        }
        break;
    case 4:
        if (strcmp(regs[mul_cdb.dest].tag , mul_cdb.tag) == 0) {
            regs[mul_cdb.dest].valid = 1;
            regs[mul_cdb.dest].value = mul_cdb.data;
            strcpy(regs[mul_cdb.dest].tag, "");
        }
        trace_cdb[trace_cdb_count].cycle = clock;
        trace_cdb[trace_cdb_count].pc = pc_selected;
        trace_cdb[trace_cdb_count].cdb_type = 4;
        trace_cdb[trace_cdb_count].data = mul_cdb.data;
        strcpy(trace_cdb[trace_cdb_count].tag, mul_cdb.tag);
        trace_cdb_count += 1;
        for (int i = 0; i < add_nr_reservation; i++)
        {
            if (add_reservation[i].busy && add_reservation[i].num_func_unit == -1) {
                if (strcmp(add_reservation[i].Qj , mul_cdb.tag) == 0) {add_reservation[i].Vj = mul_cdb.data; strcpy(add_reservation[i].Qj, "");}
                if (strcmp(add_reservation[i].Qk , mul_cdb.tag) == 0) {add_reservation[i].Vk = mul_cdb.data; strcpy(add_reservation[i].Qk, "");}
            }
        }
        for (int i = 0; i < mul_nr_reservation; i++)
        {
            if (mul_reservation[i].busy && mul_reservation[i].num_func_unit == -1) {
                if (strcmp(mul_reservation[i].Qj , mul_cdb.tag) == 0) {mul_reservation[i].Vj = mul_cdb.data; strcpy(mul_reservation[i].Qj, "");}
                if (strcmp(mul_reservation[i].Qk , mul_cdb.tag) == 0) {mul_reservation[i].Vk = mul_cdb.data; strcpy(mul_reservation[i].Qk, "");}
            }
        }
        for (int i = 0; i < div_nr_reservation; i++)
        {
            if (div_reservation[i].busy && div_reservation[i].num_func_unit == -1) {
                if (strcmp(div_reservation[i].Qj , mul_cdb.tag) == 0) {div_reservation[i].Vj = mul_cdb.data; strcpy(div_reservation[i].Qj, "");}
                if (strcmp(div_reservation[i].Qk , mul_cdb.tag) == 0) {div_reservation[i].Vk = mul_cdb.data; strcpy(div_reservation[i].Qk, "");}
            }
        }
        break;
    case 5:
        if (strcmp(regs[div_cdb.dest].tag , div_cdb.tag) == 0) {
            regs[div_cdb.dest].valid = 1;
            regs[div_cdb.dest].value = div_cdb.data;
            strcpy(regs[div_cdb.dest].tag, "");
        }
        trace_cdb[trace_cdb_count].cycle = clock;
        trace_cdb[trace_cdb_count].pc = pc_selected;
        trace_cdb[trace_cdb_count].cdb_type = 5;
        trace_cdb[trace_cdb_count].data = div_cdb.data;
        strcpy(trace_cdb[trace_cdb_count].tag, div_cdb.tag);
        trace_cdb_count += 1;
        for (int i = 0; i < add_nr_reservation; i++)
        {
            if (add_reservation[i].busy && add_reservation[i].num_func_unit == -1) {
                if (strcmp(add_reservation[i].Qj , div_cdb.tag) == 0) {add_reservation[i].Vj = div_cdb.data; strcpy(add_reservation[i].Qj, "");}
                if (strcmp(add_reservation[i].Qk , div_cdb.tag) == 0) {add_reservation[i].Vk = div_cdb.data; strcpy(add_reservation[i].Qk, "");}
            }
        }
        for (int i = 0; i < mul_nr_reservation; i++)
        {
            if (mul_reservation[i].busy && mul_reservation[i].num_func_unit == -1) {
                if (strcmp(mul_reservation[i].Qj , div_cdb.tag) == 0) {mul_reservation[i].Vj = div_cdb.data; strcpy(mul_reservation[i].Qj, "");}
                if (strcmp(mul_reservation[i].Qk , div_cdb.tag) == 0) {mul_reservation[i].Vk = div_cdb.data; strcpy(mul_reservation[i].Qk, "");}
            }
        }
        for (int i = 0; i < div_nr_reservation; i++)
        {
            if (div_reservation[i].busy && div_reservation[i].num_func_unit == -1) {
                if (strcmp(div_reservation[i].Qj , div_cdb.tag) == 0) {div_reservation[i].Vj = div_cdb.data; strcpy(div_reservation[i].Qj, "");}
                if (strcmp(div_reservation[i].Qk , div_cdb.tag) == 0) {div_reservation[i].Vk = div_cdb.data; strcpy(div_reservation[i].Qk, "");}
            }
        }
        break;
    default:
        printf("cdb_write:  invalid opcode %d , can only get 2,4,5", type);
        break;
    }
}

void is_done() {    // a function we call only if halt=1, in order to finish simulations only when all instructions before halt are done
    int add_done = 1;
    int mul_done = 1;
    int div_done = 1;
    for (int i = 0; i < add_nr_reservation; i++) {
        if (add_reservation[i].busy) {
            add_done = 0;
        }
    }
    for (int i = 0; i < mul_nr_reservation; i++) {
        if (mul_reservation[i].busy) {
            mul_done = 0;
        }
    }
    for (int i = 0; i < div_nr_reservation; i++) {
        if (div_reservation[i].busy) {
            div_done = 0;
        }
    }
    done = (add_done && mul_done && div_done && queue_is_empty());
}
// ------------------------------------------------------------------ Pipe function --------------------------------------------------------------------------

int fetch() {
    for (int i = 0; i < 2; i++)
    {
        if ((inst_arr[inst_pc].opcode < 2) || (inst_arr[inst_pc].opcode > 6)) // opcode invalid - no instructions to fetch
        {
            return inst_pc;
        }
        if (inst_arr[inst_pc].opcode == 6) { // fetched halt instruction - stop fetching instructions
            halt=1;
            return inst_pc;
        }
        int succeed = enqueue_inst_q(&inst_arr[inst_pc], inst_pc);
        inst_pc += succeed;
    }
    return inst_pc;
}

void issue() {
    for (int i = 0; i < 2; i++)
    {
        if (queue_is_empty()) return;
        inst* instruction =instruction_queue.inst_q[instruction_queue.front];
        if (instruction_queue.inst_q[instruction_queue.front]->clk_fetched == clock) // instruction was fetched in the same clock cycle
        {
            // Cannot issue. instruction was fetched in the same clock cycle
            return; //return because if instruction on the front of the queue just fetched, its true for the next one too
        }
        int tag = into_res_staion(instruction); // adding inst to res station
        if (tag != -1) { // if succeded
            instruction = dequeue(); // dequeue because the inst was sent to station
            if (instruction->opcode == NAN) printf("ERROR in dequeue!");
            regs[instruction->dst].valid = 0;
            switch (instruction->opcode)
                    {
                    case 2:
                    case 3:
                        strcpy(instruction->tag , add_reservation[tag].name); 
                        strcpy(regs[instruction->dst].tag, add_reservation[tag].name);
                        break;
                    case 4:
                        strcpy(instruction->tag , mul_reservation[tag].name); 
                        strcpy(regs[instruction->dst].tag, mul_reservation[tag].name);
                        break;
                    case 5:
                        strcpy(instruction->tag , div_reservation[tag].name); 
                        strcpy(regs[instruction->dst].tag, div_reservation[tag].name);
                        break;
                    case 6:
                        // halt 
                        break;
                    default:
                        printf(" ERROR: instruction.opcode invalid\n");
                        break;
                    }
            
            instruction->clk_issued = clock; // need it?
            inst_arr[instruction->pc].clk_issued = clock; // update clk_issued for this instruction in inst_arr
        }
    }
}

void execute() {
    if (free_add_units>0) {
        // temp array to hold res_lines from add reservation unit that are ready for execution
        res_station* ready_to_execute_add = malloc(add_nr_reservation * sizeof(res_station));
        int num_ready_add = 0;
        // Iterate over add reservation stations
        for (int i = 0; i < add_nr_reservation; i++) {
            if (is_ready_for_execution(&add_reservation[i])) {
                ready_to_execute_add[num_ready_add++] = add_reservation[i];
            }
        }
        if (free_add_units<num_ready_add) {
            //There are more ready add instructions then free units - sort the ready instructions by their pc
            qsort(ready_to_execute_add, free_add_units, sizeof(res_station*), compare_pc);
        }
        if (num_ready_add >0) {
            execute_inst(ready_to_execute_add, num_ready_add, &free_add_units);
        } else {
            //No ADD instructions ready to execute in this cycle
        }
        free(ready_to_execute_add);
    }
    
    if (free_mul_units>0) {
        res_station *ready_to_execute_mul = malloc(mul_nr_reservation * sizeof(res_station));
        int num_ready_mul = 0;
        // Iterate over mul reservation stations
        for (int i = 0; i < mul_nr_reservation; i++) {
            if (is_ready_for_execution(&mul_reservation[i])) {
                ready_to_execute_mul[num_ready_mul++] = mul_reservation[i];
            }
        }
        if (free_mul_units<num_ready_mul) {
            //There are more ready mul instructions then free units
            qsort(ready_to_execute_mul, free_mul_units, sizeof(res_station*), compare_pc);
        }
        if (num_ready_mul >0) {
            execute_inst(ready_to_execute_mul, num_ready_mul, &free_mul_units);
        } else {
            //No MUL instructions ready to execute in this cycle
        }
        free(ready_to_execute_mul);
    }
    if (free_div_units>0) {
        res_station *ready_to_execute_div = malloc(div_nr_reservation * sizeof(res_station));
        int num_ready_div = 0;
        // Iterate over div reservation stations
        for (int i = 0; i < div_nr_reservation; i++) {
            if (is_ready_for_execution(&div_reservation[i])) {
                ready_to_execute_div[num_ready_div++] = div_reservation[i];
            }
        }
        if (free_div_units<num_ready_div) {
            //There are more ready div instructions then free units
            qsort(ready_to_execute_div, free_div_units, sizeof(res_station*), compare_pc);
        }
        if (num_ready_div >0) {
            execute_inst(ready_to_execute_div, num_ready_div, &free_div_units);
        } else {
            //No DIV instructions ready to execute in this cycle
        }
        free(ready_to_execute_div);
    }
}

void write_result() {
    // writing in add_cdb
    int add_inst_selected = -1;
    int add_pc_selected = -1;
    for (int i = 0; i < add_nr_reservation; i++) // look for instruction that is done executing and ready to be written to cdb
    {
        if (add_reservation[i].busy && add_reservation[i].num_func_unit != -1) {
            if ((inst_arr[add_reservation[i].pc].clk_end < clock)) { // still on execute 
                if (add_pc_selected == -1) {
                    add_inst_selected = i;
                    add_pc_selected = add_reservation[i].pc;
                }
                else {
                    if (add_reservation[i].pc < add_pc_selected) {
                        add_inst_selected = i;
                        add_pc_selected = add_reservation[i].pc;
                    }
                }
            }
        }
    }
    if (add_inst_selected != -1)
    {
        inst_arr[add_pc_selected].clk_write = clock;
        add_units[add_reservation[add_inst_selected].num_func_unit].busy = false;
        clear_station(2, add_inst_selected);
        free_add_units += 1;
        strcpy(add_cdb.tag, add_reservation[add_inst_selected].name);
        add_cdb.data = inst_arr[add_pc_selected].result;
        add_cdb.dest = inst_arr[add_pc_selected].dst;
        cdb_write(2, add_pc_selected);
    }

    // writing in mul_cdb
    int mul_inst_selected = -1;
    int mul_pc_selected = -1;
    for (int i = 0; i < mul_nr_reservation; i++)
    {
        if (mul_reservation[i].busy && mul_reservation[i].num_func_unit != -1)
        {
            if ((inst_arr[mul_reservation[i].pc].clk_end < clock)) {
                if (mul_pc_selected == -1)
                {
                    mul_inst_selected = i;
                    mul_pc_selected = mul_reservation[i].pc;
                }
                else
                {
                    if (mul_reservation[i].pc < mul_pc_selected)
                    {
                        mul_inst_selected = i;
                        mul_pc_selected = mul_reservation[i].pc;
                    }
                }
            }
        }
    }
    if (mul_inst_selected != -1)
    {
        inst_arr[mul_pc_selected].clk_write = clock;
        mul_units[mul_reservation[mul_inst_selected].num_func_unit].busy = false;
        clear_station(4, mul_inst_selected);
        free_mul_units += 1;
        strcpy(mul_cdb.tag, mul_reservation[mul_inst_selected].name);
        mul_cdb.data = inst_arr[mul_pc_selected].result;
        mul_cdb.dest = inst_arr[mul_pc_selected].dst;
        cdb_write(4, mul_pc_selected);
    }

    // writing in div_cdb
    int div_inst_selected = -1;
    int div_pc_selected = -1;
    for (int i = 0; i < div_nr_reservation; i++)
    {
        if (div_reservation[i].busy && div_reservation[i].num_func_unit != -1)
        {
            if ((inst_arr[div_reservation[i].pc].clk_end < clock)) {
                if (div_pc_selected == -1)
                {
                    div_inst_selected = i;
                    div_pc_selected = div_reservation[i].pc;
                }
                else
                {
                    if (div_reservation[i].pc < div_pc_selected)
                    {
                        div_inst_selected = i;
                        div_pc_selected = div_reservation[i].pc;
                    }
                }
            }
        }
    }
    if (div_inst_selected != -1)
    {
        inst_arr[div_pc_selected].clk_write = clock;
        div_units[div_reservation[div_inst_selected].num_func_unit].busy = false;
        clear_station(5, div_inst_selected);
        free_div_units += 1;
        strcpy(div_cdb.tag, div_reservation[div_inst_selected].name);
        div_cdb.data = inst_arr[div_pc_selected].result;
        div_cdb.dest = inst_arr[div_pc_selected].dst;
        cdb_write(5, div_pc_selected);
    }
}

//--------------------------------------------------------------------- MAIN -----------------------------------------------------------------------------------------------

int main(int argc,char* argv[]) {
     // ------------Initialization-------------
    if (argc < 6) { // decide if we want this checker
        printf("Wrong usage. In order to run simulator you must provide all arguments:");
        printf("sim.exe cfg.txt memin.txt regout.txt traceinst.txt tracecdb.txt");
        return 1;
     }
    config(argv[1]); //consider setting return value to be a checker that all done smoothly
    // initialize number of free functional units to number of units
    free_add_units = add_nr_units;
    free_mul_units = mul_nr_units;
    free_div_units = div_nr_units;
    memin_read(argv[2]); // read file memin.txt to the array inst_arr
    // build the pipe
    initial_queue(); // setting front and rear to -1
    add_units = malloc(add_nr_units * sizeof(func_unit));
    mul_units = malloc(mul_nr_units * sizeof(func_unit));
    div_units = malloc(div_nr_units * sizeof(func_unit));
    build_functional_units();  
    add_reservation = malloc(add_nr_reservation * sizeof(res_station));
    mul_reservation = malloc(mul_nr_reservation * sizeof(res_station));
    div_reservation = malloc(div_nr_reservation * sizeof(res_station));
    build_reservation_station(); // self-chack: the pointer working

    create_regs(); // initialize registers values
    init_cdb(); // init values for 3 CDB's, and trace_cdb_count

    // // ---------------Starting to run the program---------------
    while (!halt || !done) {
        if (!halt) inst_pc = fetch(); // insert up to 2 new instructions to queue from inst_mem 
        issue();
        execute();
        write_result();
        clock += 1;
        if (halt) is_done();
    }

    // ---------------exiting sequence---------------
    // printing traces
    // freeing memory
    // exiting
    print_regout(argv[3]); //printing reg values to file
    print_trace_inst(argv[4]); //printing inst trace to file
    print_trace_cdb(argv[5]); //printing CDB trace to file

    free(add_units);
    free(mul_units);
    free(div_units);

    free(add_reservation);
    free(mul_reservation);
    free(div_reservation);

    return 0;
}