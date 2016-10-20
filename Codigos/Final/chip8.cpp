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
                                             

INSTALANDO DEPENDENCIAS (SFML):
Ubuntu: "sudo apt-get install libsfml-dev"
Fedora: "sudo yum install sfml"
Arch:   "sudo pacman -S sfml"
Outros sistemas, consulte: http://www.sfml-dev.org/download/sfml/2.4.0/

COMPILE:
"chmod +x build.sh"
"./build.sh"

EXECUTE:
"./emulator nome_da_rom"
*****************************************************************************/

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//definições
#define WINDOW_SCALE   15 //para que a janela não seja muito pequena
#define EMULATOR_SPEED 6  //controla a velocidade do emulador (chip-8 não possui clock definido)

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

//sfml
sf::RenderWindow   window(sf::VideoMode(displayWidth*WINDOW_SCALE, displayHeight*WINDOW_SCALE), "Chip-8 Emulator", sf::Style::Close);
sf::SoundBuffer    buffer;
sf::Sound          sound;

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

/* Inicializa SFML */
void sfmlStartup() {
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60); //garante 60hz

    buffer.loadFromFile("sound/beep.wav"); //carrega um exemplo de som
    sound.setBuffer(buffer);
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

/* Limpa a memória gráfica (tela) */
void clearDisplay() {
    for (int i = 0; i < displayWidth*displayHeight; i++) { 
        display[i] = 0;
    }
}

/*  Desenha um sprite de uma determinado altura e largura 8 na tela a partir da coord (x,y).
 *  A localização do sprite é endereçada pelo registrador I. 
 */
void draw(byte x, byte y, byte height) {
    word pixel;
 
    x = x%64;
    y = y%32;

    V[0xF] = 0;
    for (int yline = 0; yline < height; yline++) {
        pixel = memory[I + yline];
        for (int xline = 0; xline < 8; xline++) {
            if ((pixel & (0x80 >> xline)) != 0) {
                if (display[(x + xline + ((y + yline) * 64))] == 1) {
                    V[0xF] = 1;                                 
                }
                display[x + xline + ((y + yline) * 64)] ^= 1;
            }
        }
    }
}

/* Converte o valor de Vx para BCD e grava na memória a partir do endereço em I */
void storeBCD(unsigned char& Vx) {
    memory[I]     = Vx / 100;
    memory[I + 1] = (Vx / 10) % 10;
    memory[I + 2] = (Vx % 100) % 10;
}

/* Espera por uma tecla ser pressionada */
void waitKey(byte& Vx) {
    bool pressed = false;

    for (int i = 0; i<0xF; i++) { 
        if (key[i]) { 
            Vx = i;
            pressed = true;
        }
    }

    if (!pressed) {
        PC -= 2;
    }
}

/* Lê os registradores da memória */
void readRegistersFromMem(byte x) {
    for (int i = 0; i <= x; ++i) {
        V[i] = memory[I + i];
    }

    I += x + 1;
}

/* Escreve os registradores na memória */
void writeRegistersToMem(byte x) {
    for (int i = 0; i <= x; ++i) {
        memory[I + i] = V[i];
    }

    I += x + 1;
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
                	printf("CLS");
                	clearDisplay();
                    break;
                case 0xEE: //RET
                	printf("RET");
                	SP--;
                    PC = stack[SP];
                    break;
                case 0xFD: //EXIT
                	printf("EXIT\n");
                	printMemoryFile();
                	exit(0);
                	break;
                default:  //SYS addr
                	printf("SYS 0x%x (ignoring)", nnn);
                    break;
            }
            break;
        case 0x1: //JP addr
            printf("JP 0x%x", nnn);
            PC = nnn;
            break;
        case 0x2: //CALL addr
            printf("CALL 0x%x", nnn);
            stack[SP] = PC;
            SP++;
            PC = nnn;
            break;
        case 0x3: //SE Vx, byte
            printf("SE V%x, #%d", x, (sbyte) kk);
            if (V[x] == kk) {
                PC += 2;
            }
            break;
        case 0x4: //SNE Vx, byte
            printf("SNE V%x, #%d", x, (sbyte) kk);
            if (V[x] != kk) {
                PC += 2;
            }
            break;
        case 0x5: //SE Vx, Vy
            printf("SE V%x, V%x", x, y);
            if (V[x] == V[y]) {
                PC += 2;
            }
            break;
        case 0x6: //LD Vx, byte
        	printf("LD V%x, #%d", x, (sbyte) kk);
            V[x] = kk;
            break;
        case 0x7: //ADD Vx, byte
            printf("ADD V%x, #%d", x, kk);
            V[x] += kk;
            break;
        case 0x8:
            switch (n) {
                case 0x0: //LD  Vx, Vy
                    printf("LD V%x, V%x", x, y);
                    V[x] = V[y];
                    break;
                case 0x1: //OR  Vx, Vy
                    printf("OR V%x, V%x", x, y);
                    V[x] |= V[y];
                    break;
                case 0x2: //AND Vx, Vy
                    printf("AND V%x, V%x", x, y);
                    V[x] &= V[y];
                    break;
                case 0x3: //XOR Vx, Vy
                    printf("XOR V%x, V%x", x, y);
                    V[x] ^= V[y];
                    break;
                case 0x4: //ADD Vx, Vy
                    printf("ADD V%x, V%x", x, y);
                    tmp = V[x] + V[y];
                    V[0xF] = (tmp >> 8);
                    V[x] = tmp;
                    break;
                case 0x5: //SUB Vx, Vy
                	printf("SUB V%x, V%x", x, y);
                	tmp = V[x] - V[y];
                    V[0xF] = !(tmp >> 8);
                    V[x] = tmp;
                    break;
                case 0x6: //SHR Vx {, Vy}
                    printf("SHR V%x {, V%x}", x, y);
                    V[0xF] = V[y] & 1;
                    V[x] = V[y] << 1;
                    break;
                case 0x7: //SUBN Vx, Vy
                    printf("SUBN V%x, V%x", x, y);
                    tmp = V[y] - V[x];
                    V[0xF] = !(tmp >> 8);
                    V[x] = tmp;
                    break;
                case 0xE: //SHL Vx {, Vy}
                    printf("SHL V%x {, V%x}", x, y);
                    V[0xF] = V[y] >> 7;
                    V[x] = V[y] >> 1;
                    break;
                default:
                    printf("Invalid opcode!\n");
                    exit(1);
            }
            break;
        case 0x9: //SNE Vx, Vy
            printf("SNE V%x, V%x", x, y);
            if (V[x] != V[y]) {
                PC += 2;
            }
            break;
        case 0xA: //LD I, addr
            printf("LD I, 0x%x", nnn);
            I = nnn;
            break;
        case 0xB: //JP V0, addr
            printf("JP V0, 0x%x", nnn);
            PC = V[0] + nnn;
            break;
        case 0xC: //RND Vx, byte
            printf("RND V0, 0x%x", (sbyte) kk);
            V[x] = ( rand()%0xF & kk );
            break;
        case 0xD: //DRW Vx, Vy, nibble
            printf("DRW V%x, V%x, 0x%x", x, y, n);
            draw(V[x],V[y],n);
            break;
        case 0xE:
            switch (kk) {
                case 0x9E: //SKP Vx
                    printf("SKP V%x", x);
                    if (key[V[x]]) {
                        PC += 2;
                    }
                    break;
                case 0xA1: //SKNP Vx
                    printf("SKNP V%x", x);
                    if (!key[V[x]]) {
                        PC += 2;
                    }
                    break;
                default:
                    printf("Invalid opcode!\n");
                    exit(1);
            }
            break;
        case 0xF:
            switch (kk) {
                case 0x07: //LD Vx, DT
                    printf("LD V%x, DT", x);
                    V[x] = delayTimer;
                    break;
                case 0x0A: //LD Vx, K
                    printf("LD V%x, K", x);
                    waitKey(V[x]);
                    break;
                case 0x15: //LD DT, Vx
                    printf("LD DT, V%x", x);
                    delayTimer = V[x];
                    break;
                case 0x18: //LD ST, Vx
                    printf("LD ST, V%x", x);
                    soundTimer = V[x]; 
                    break;
                case 0x1E: //ADD I, Vx
                    printf("ADD I, V%x", x);
                    I += V[x];
                    break;
                case 0x29: //LD F, Vx
                    printf("LD F, V%x", x);
                    I = V[x]*0x5;
                    break;
                case 0x33: //LD B, Vx
                    printf("LD B, V%x", x);
                    storeBCD(V[x]);
                    break;
                case 0x55: //LD [I], Vx
                    printf("LD [I], V%x", x);
                    writeRegistersToMem(x);
                    break;
                case 0x65: //LD Vx, [I]
                    printf("LD V%x. [I]", x);
                    readRegistersFromMem(x); 
                    break;
                default:
                    printf("Invalid opcode!\n");
                    exit(1);
            }
            break;
    }

    //atualiza os timers
    if (delayTimer > 0)
        delayTimer--;
    if (soundTimer > 0) {
        sound.play(); //reproduz o beep
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

    //inicializa o SFML
    sfmlStartup();

    //carrega a ROM na memória
    loadROM(argv[1]);

    //cria uma imagem para atualizar a tela
    sf::Image image;
    image.create(displayWidth, displayHeight, sf::Color::Black);

    printHeader(); //exibi um header dos registradores
    while (window.isOpen()) {
        //verifica por teclas pressionadas, atualizando o vetor de teclado ("keys")
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                switch (event.key.code) {
                    case (sf::Keyboard::Q): key[0x0] = 1; break;
                    case (sf::Keyboard::A): key[0x1] = 1; break;
                    case (sf::Keyboard::Z): key[0x2] = 1; break;
                    case (sf::Keyboard::W): key[0x3] = 1; break; 
                    case (sf::Keyboard::S): key[0x4] = 1; break; 
                    case (sf::Keyboard::X): key[0x5] = 1; break; 
                    case (sf::Keyboard::E): key[0x6] = 1; break; 
                    case (sf::Keyboard::D): key[0x7] = 1; break; 
                    case (sf::Keyboard::C): key[0x8] = 1; break; 
                    case (sf::Keyboard::R): key[0x9] = 1; break; 
                    case (sf::Keyboard::F): key[0xA] = 1; break;
                    case (sf::Keyboard::V): key[0xB] = 1; break; 
                    case (sf::Keyboard::T): key[0xC] = 1; break; 
                    case (sf::Keyboard::G): key[0xD] = 1; break; 
                    case (sf::Keyboard::B): key[0xE] = 1; break; 
                    case (sf::Keyboard::N): key[0xF] = 1;
                }
            } else if (event.type == sf::Event::KeyReleased) {
                switch (event.key.code) {
                    case (sf::Keyboard::Q): key[0x0] = 0; break;
                    case (sf::Keyboard::A): key[0x1] = 0; break;
                    case (sf::Keyboard::Z): key[0x2] = 0; break;
                    case (sf::Keyboard::W): key[0x3] = 0; break; 
                    case (sf::Keyboard::S): key[0x4] = 0; break; 
                    case (sf::Keyboard::X): key[0x5] = 0; break; 
                    case (sf::Keyboard::E): key[0x6] = 0; break; 
                    case (sf::Keyboard::D): key[0x7] = 0; break; 
                    case (sf::Keyboard::C): key[0x8] = 0; break; 
                    case (sf::Keyboard::R): key[0x9] = 0; break; 
                    case (sf::Keyboard::F): key[0xA] = 0; break;
                    case (sf::Keyboard::V): key[0xB] = 0; break; 
                    case (sf::Keyboard::T): key[0xC] = 0; break; 
                    case (sf::Keyboard::G): key[0xD] = 0; break; 
                    case (sf::Keyboard::B): key[0xE] = 0; break; 
                    case (sf::Keyboard::N): key[0xF] = 0;
                }
            } 
        }

        //executa as instruções
        for (int i = 0; i < EMULATOR_SPEED; i++) {
            emulateCycle();
        }

        //desenha na tela
        window.clear();
            for (int i = 0; i < displayHeight; i++) {
                for (int j = 0; j < displayWidth; j++) {
                    if (display[j + i*displayWidth] == 1) {
                    	image.setPixel(j, i, sf::Color::Red);
                    } else {
                    	image.setPixel(j, i, sf::Color::Black);
                    }
                }
            }
            sf::Texture texture;
		    texture.loadFromImage(image);
		    sf::Sprite sprite;
		    sprite.setTexture(texture);
		    sprite.setScale(sf::Vector2f(WINDOW_SCALE, WINDOW_SCALE));
            window.draw(sprite);
        window.display();
    }

    return 0;
}
