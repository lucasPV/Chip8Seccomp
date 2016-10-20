/****************************************************************************
  ____ _   _ ___ ____  ___  
 / ___| | | |_ _|  _ \( _ ) 
| |   | |_| || || |_) / _ \ 
| |___|  _  || ||  __/ (_) |
 \____|_| |_|___|_|   \___/ 
 _____                 _       _             
| ____|_ __ ___  _   _| | __ _| |_ ___  _ __ 
|  _| | '_ ` _ \| | | | |/ _` | __/ _ \| '__|
| |___| | | | | | |_| | | (_| | || (_) | |   
|_____|_| |_| |_|\__,_|_|\__,_|\__\___/|_|   
                                             

No código abaixo, a estrutura geral do emulador está montada.
No entanto, uma parte importante do emulador ainda está
faltando: a implementação das instruções.

ATIVIDADE PRÁTICA:
A partir da documentação, implemente as instruções que faltam,
completando o switch das linhas 215 a 350. Remova a chamada da função 
notImplemented e insira sua implementação.
Apesar de não ser necessário, é altamente recomendável que se escreva códigos
em linguagem de montagem para testar o funcionamento do emulador em construção.
Boa Sorte!

DOCUMENTAÇÕES:
http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
https://en.wikipedia.org/wiki/CHIP-8

ASSEMBLER:
https://github.com/aranega/chip8-compiler

MEU GITHUB:
https://github.com/lucasPV

OBSERVAÇÕES:
Para maior particidade, use GNU/Linux.
Compile o código com g++ usando o comando "g++ chip8.cpp -o emulator".
Chame o emulador usando o comando "./emulator nome_da_ROM"
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//tipos
typedef unsigned char  byte;
typedef unsigned short word;
typedef signed char    sbyte;
typedef signed short   sword;

//memória principal
const int memSize  = 0xFFF;   //4KB
const int fontSize = 0x200;   //512B são utilizados para armazenar as fontes
byte memory[memSize];         //armazena as fontes e a ROM

//memória gráfica (tela)
const int displayWidth  = 64;
const int displayHeight = 32;
byte display[displayWidth*displayHeight];

//teclado
byte key[17];

//registradores
byte V[0xF];                  //V0-VE: Propósito Geral; VF: Carry, Borrow e Detectção de Colisões
word I, PC, SP;               //I: Registrador de índice; PC: Contador de Programa; SP: Stack Pointer (Ponteiro da Pilha)
byte delayTimer, soundTimer;  //registradores de timer que contam a 60hz

//pilha de chamadas (call stack)
const int stackLevels = 16;
word stack[stackLevels];

//conjunto de fontes (fontset)
const byte fontset[80] = { 
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

/* Carrega a ROM na memória */
void loadROM(const char* filename) {
    //open file
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Couldn't open ROM: %s\n", filename);
        exit(1);
    }

    //get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size > (memSize-fontSize)) {
        printf("ROM too big for Chip8 Memory (more than 3.5KB)!\n");
        exit(1);
    }

    //read file into memory
    fread(memory + fontSize, sizeof(byte), size, file);

    //close
    fclose(file);
}

/* Inicializa as estruturas (análogo a um "boot") */
void startup() {
    srand(time(NULL));  //seed para números aleatórios

    PC = 0x200;         //o sistema espera que a ROM esteja carregada em 0x200
    I  = 0;             //inicializa o registrador de índice
    SP = 0;             //inicializa o ponteiro da pilha
    delayTimer = 0;     //inicializa o delay timer
    soundTimer = 0;     //inicializa o timer de som

    //inicializa os registradores V0-VF
    for (int i = 0; i < 0xF; i++) {
        V[i] = 0;
    }

    //limpa a pilha de chamadas
    for (int i = 0; i < stackLevels; i++) {
        stack[i] = 0;
    }

    //carrega o fontset na memória
    for (int i = 0; i < 80; i++) { 
        memory[i] = fontset[i];
    }

    //limpa a memória gráfica
    for (int i = 0; i < displayWidth*displayHeight; i++) {
        display[i] = 0;
    }

    //limpa o teclado
    for (int i = 0; i < 0xF; i++) {
        key[i] = 0;
    }
}

/* Imprime uma mensagem de que a instrução não foi implementada e aborta a emulação */
void notImplemented(word instr) {
    printf("Instruction %.4x couldn't be interpreted! Aborting emulation...\n", instr);
    exit(1);
}

/* Imprime um cabeçalho com o nome dos registradores para fins de debug */
void printHeader() {
	printf("PC  \tINSTR\t\tV0   V1   V2   V3   V4   V5   V6   V7   V8   V9   VA   VB   VC   VD   VE   VF   I     SP\n");
}

/* Imprime o valor dos registradores */
void printState() {
	printf("\t\t\t");
    for (int i = 0; i <= 0xF; i++) {
    	sbyte value = V[i];
    	if (value >= 0) {
    		printf("%.3d  ", value);
    	} else {
    		printf("%.3d ", value);
    	}
    }
    printf("$%.4x $%.4x\n", I, SP);
}

/* Exporta os dados da memória em um arquivo externo (memory.txt) para fins de debug */
void printMemoryFile() {
	FILE* file = fopen("memory.txt","w");

    for (int i = 0; i < memSize; i++) {
        fprintf(file, "[0x%.4x]\t0x%.4x\n", i, memory[i]);
    }

    fclose(file);
}

/* Emula um ciclo do chip8 (busca, decodifica e executa uma instrução) */
void emulateCycle() {
	//antes de mais nada, vamos exibir informações para debug
	printState();
	printf("$%.4x\t", PC);

    //busca de instrução
    word instr = ((memory[PC] << 8) | memory[PC + 1]);

    //atualiza PC
    PC += 2;

    //extrai os bits da instrução
    byte p   = ((instr & 0xF000) >> 12);
    byte x   = ((instr & 0x0F00) >> 8);
    byte y   = ((instr & 0x00F0) >> 4);
    byte kk  = (instr & 0x00FF);
    word nnn = (instr & 0x0FFF);
    byte n   = (instr & 0x000F);

    //aloca-se uma variável temporária para executar as operações
    word tmp;

    //executa a instrução
    switch (p) {
        case 0x0:
            switch (kk) {
                case 0xE0: //CLS
                    notImplemented(instr);
                    break;
                case 0xEE: //RET
                    notImplemented(instr);
                    break;
                case 0xFD: //EXIT
                	printf("EXIT\n");
                	printMemoryFile();
                	exit(0);
                	break;
                default:  //SYS addr
                    notImplemented(instr);
                    break;
            }
            break;
        case 0x1: //JP addr
            notImplemented(instr);
            break;
        case 0x2: //CALL addr
            notImplemented(instr);
            break;
        case 0x3: //SE Vx, byte
            notImplemented(instr);
            break;
        case 0x4: //SNE Vx, byte
            notImplemented(instr);
            break;
        case 0x5: //SE Vx, Vy
            notImplemented(instr);
            break;
        case 0x6: //LD Vx, byte
        	printf("LD V%x, #%d", x, (sbyte) kk);
            V[x] = kk;
            break;
        case 0x7: //ADD Vx, byte
            notImplemented(instr);
            break;
        case 0x8:
            switch (n) {
                case 0x0: //LD  Vx, Vy
                    notImplemented(instr);
                    break;
                case 0x1: //OR  Vx, Vy
                    notImplemented(instr);
                    break;
                case 0x2: //AND Vx, Vy
                    notImplemented(instr);
                    break;
                case 0x3: //XOR Vx, Vy
                    notImplemented(instr);
                    break;
                case 0x4: //ADD Vx, Vy
                    notImplemented(instr);
                    break;
                case 0x5: //SUB Vx, Vy
                	notImplemented(instr);
                    break;
                case 0x6: //SHR Vx {, Vy}
                    notImplemented(instr);
                    break;
                case 0x7: //SUBN Vx, Vy
                    notImplemented(instr);
                    break;
                case 0xE: //SHL Vx {, Vy}
                    notImplemented(instr);
                    break;
                default:
                    notImplemented(instr);
            }
            break;
        case 0x9: //SNE Vx, Vy
            notImplemented(instr);
            break;
        case 0xA: //LD I, addr
            notImplemented(instr);
            break;
        case 0xB: //JP V0, addr
            notImplemented(instr);
            break;
        case 0xC: //RND Vx, byte
            notImplemented(instr);
            break;
        case 0xD: //DRW Vx, Vy, nibble
            notImplemented(instr);
            break;
        case 0xE:
            switch (kk) {
                case 0x9E: //SKP Vx
                    notImplemented(instr);
                    break;
                case 0xA1: //SKNP Vx
                    notImplemented(instr);
                    break;
                default:
                    notImplemented(instr);
            }
            break;
        case 0xF:
            switch (kk) {
                case 0x07: //LD Vx, DT
                    notImplemented(instr);
                    break;
                case 0x0A: //LD Vx, K
                    notImplemented(instr);
                    break;
                case 0x15: //LD DT, Vx
                    notImplemented(instr);
                    break;
                case 0x18: //LD ST, Vx
                    notImplemented(instr);
                    break;
                case 0x1E: //ADD I, Vx
                    notImplemented(instr);
                    break;
                case 0x29: //LD F, Vx
                    notImplemented(instr);
                    break;
                case 0x33: //LD B, Vx
                    notImplemented(instr);
                    break;
                case 0x55: //LD [I], Vx
                    notImplemented(instr);
                    break;
                case 0x65: //LD Vx, [I]
                    notImplemented(instr);
                    break;
                default:
                    notImplemented(instr);
            }
            break;
    }

    //atualiza os timers
    if (delayTimer > 0)
        delayTimer--;
    if (soundTimer > 0) {
        //quando tivermos sfml, devemos tocar o som aqui
        soundTimer--;
    }

    printf("\n");
}

int main(int argc, char* argv[]) {
	//verificar se ROM foi passada como parâmetro
    if (argc < 2) {
        printf("ROM not specified!\n");
        exit(1);
    }

    //inicializar estruturas
    startup();

    //carrega a ROM na memória
    loadROM(argv[1]);

    printHeader(); //exibi um header dos registradores
    while (1) {
        emulateCycle(); //executa instruções até encontrar EXIT
    }

    return 0;
}
