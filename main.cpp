//Yes, one file is enough.

#include <fstream>
#include <SDL/SDL.h>
#include <math.h>
#include <windows.h>
#include <time.h>
#include <string.h>

#define PIXEL_SIZE 10
#define X_RESOL 64
#define Y_RESOL 32
#define FPS 60
#define CPU_FREQUENCY 600

using namespace std;

struct Registers
{
    Uint8 r[16];
    int DT;
    int ST;
    Uint16 I;
};
typedef struct Registers Registers;

struct CPU
{
    int CharOffset;
    int Subroutines[16];
    Uint8 memory[4096];
    bool inputs[16];
    Uint8 waitForKeyInReg;
    bool waitForKeyPress;
};
typedef struct CPU CPU;

void Init(CPU *proc, Registers *Reg);
int OpenGame(char *path, CPU *Proc);
bool InitSDL(SDL_Surface **screen);
void Jump(int *ptr, int addr);
void Timer(int *DT, int *ST);
void ChangePixel(SDL_Surface *surface, int x, int y, bool color);
void ClearScreen(SDL_Surface *Screen);
bool GetPixelState(int x, int y);

bool Screen[2048]; //GLOBAL VAR !

int main ( int argc, char* argv[])
{
    if (argc == 1)
    {
        printf("No file.");
        return 0;
    }

    //Initialize processor, registers and characters
    CPU Proc;
    Registers Reg;
    Init(&Proc, &Reg);

    //Initialize SDL
    SDL_Surface* ChipScreen;
    if(InitSDL(&ChipScreen))
        return 1;

    //argv[1] = "C:/Users/Jules/Documents/CodeBlocks/Echipator/bin/Debug/Games/Submarine [Carmelo Cortez, 1978].ch8";

    int FileSize = OpenGame(argv[1], &Proc); //Load game

    //End of game loading

    bool done = false;
    int ConvertKeys[16] = {SDLK_KP0, SDLK_KP7, SDLK_KP8, SDLK_KP9, SDLK_KP4, SDLK_KP5, SDLK_KP6, SDLK_KP1, SDLK_KP2, SDLK_KP3, 275, 266, SDLK_KP_MULTIPLY, SDLK_KP_MINUS, SDLK_KP_PLUS, SDLK_KP_ENTER};

    int ptr(0x200); //Instruction to execute
    Uint32 LastUpdate(0); //For screen update cadency
    Uint32 Time(0); //For processor cadency

    while (!done)
    {
        //Handle events
        SDL_Event event;
        do
        {
            while (SDL_PollEvent(&event))
            {
                if(event.type == SDL_QUIT)
                    Proc.waitForKeyPress = false;

                switch (event.type)
                {
                case SDL_QUIT:
                    done = true;
                    break;
                case SDL_KEYDOWN:
                    {

                        if(event.key.keysym.sym == SDLK_ESCAPE)
                            ClearScreen(ChipScreen);
                        if(event.key.keysym.sym == SDLK_SPACE)
                        {
                            //Reload
                            Init(&Proc, &Reg);
                            ClearScreen(ChipScreen);
                            OpenGame(argv[1], &Proc);
                            ptr = 0x200; //Instruction to execute
                            LastUpdate = 0; //For screen update cadency
                            Time = 0; //For processor cadency
                        }
                        int i(0);
                        for(i; i < 17 && ConvertKeys[i] != event.key.keysym.sym; i++);
                        if(i < 16)
                        {
                            printf("\nKey pressed: %X\n\n", i);
                            Proc.inputs[i] = 1;

                            if(Proc.waitForKeyPress)
                            {
                                Proc.waitForKeyPress = false;
                                Reg.r[Proc.waitForKeyInReg] = i;
                            }
                        }

                        break;
                    }
                case SDL_KEYUP:
                    {
                        int i(0);
                        for(i; i < 17&&ConvertKeys[i] != event.key.keysym.sym; i++ );
                        if(i<16)
                        {
                            printf("\nKey unpressed: %X\n\n", i);
                            Proc.inputs[i] = 0;
                        }
                    }
                }
            }

        }
        while(Proc.waitForKeyPress);

        //Processor
        if(SDL_GetTicks()>Time + 1000 / CPU_FREQUENCY)
        {
            //Prepare
            if(ptr >= 0x200 + FileSize + 1) break;
            Uint16 Opcode = (Proc.memory[ptr]<<8)|Proc.memory[ptr + 1];
            printf("%d: ", ptr);
            printf("%X", Opcode);


            //Interpret
            int opcodeArgs[3] = {(Opcode&0b0000111100000000)>>8, (Opcode&0b0000000011110000)>>4, Opcode&0b0000000000001111};
            switch(Opcode)
            {
            case 0x00E0:
                //00E0
                printf(" Clear screen.");
                ClearScreen(ChipScreen); //Clear screen
                break;
            case 0x00EE:
            {
                //00EE
                printf(" Return subroutine.");
                int i = 0;
                for(i;Proc.Subroutines[i] != -1; i++);
                Jump(&ptr, Proc.Subroutines[i - 1]);
                Proc.Subroutines[i-1] = -1;
                break;
            }
            default:
                switch(Opcode>>12)
                {
                case 0x1:
                    //1NNN
                    Jump(&ptr, int(opcodeArgs[0]<<8|opcodeArgs[1]<<4|opcodeArgs[2]));
                    printf(" Jump to %d", opcodeArgs[0]<<8|opcodeArgs[1]<<4|opcodeArgs[2]);
                    break;
                case 0x2:
                {
                    //2NNN
                    printf(" Subroutine %X",  opcodeArgs[0]<<8|opcodeArgs[1]<<4|opcodeArgs[2]);
                    int i = 0;
                    for(i = 0; Proc.Subroutines[i] != -1; i++);
                    Proc.Subroutines[i] = ptr + 2;
                    Jump(&ptr, int(opcodeArgs[0]<<8|opcodeArgs[1]<<4|opcodeArgs[2]));
                    break;
                }
                case 0x3:
                    //3XNN
                    printf(" Skip next instruction if V%d(%d) is equal to %d.", opcodeArgs[0], Reg.r[opcodeArgs[0]], opcodeArgs[1]<<4|opcodeArgs[2]);
                    if (Reg.r[opcodeArgs[0]] == Uint8(opcodeArgs[1]<<4|opcodeArgs[2]))
                    {
                        Jump(&ptr, ptr + 4);
                        printf(" -> Skip (%d == %d).", Reg.r[opcodeArgs[0]], opcodeArgs[1]<<4|opcodeArgs[2]);
                    }
                    break;
                case 0x4:
                    //4XNN
                    printf(" Skip next instruction if V%d(%d) is not equal to %d.", opcodeArgs[0], Reg.r[opcodeArgs[0]], opcodeArgs[1]<<4|opcodeArgs[2]);
                    if (Reg.r[opcodeArgs[0]] != Uint8(opcodeArgs[1]<<4|opcodeArgs[2]))
                    {
                        Jump(&ptr, ptr + 4);
                        printf(" -> Skip (%d != %d).", Reg.r[opcodeArgs[0]], opcodeArgs[1]<<4|opcodeArgs[2]);
                    }
                    break;
                case 0x5:
                    //5XY0
                    printf(" Skip next instruction if V%d(%d) is equal to V%d(%d).", opcodeArgs[0], Reg.r[opcodeArgs[0]], opcodeArgs[1], Reg.r[opcodeArgs[1]]);
                    if (Reg.r[opcodeArgs[0]] == Reg.r[opcodeArgs[1]])
                    {
                        Jump(&ptr, ptr + 4);
                        printf(" -> Skip (%d != %d).", Reg.r[opcodeArgs[0]], Reg.r[opcodeArgs[1]]);
                    }
                    break;
                case 0x6:
                    //6XNN
                    printf(" Store %d in V%d(%d).", opcodeArgs[1]<<4|opcodeArgs[2], opcodeArgs[0], Reg.r[opcodeArgs[0]]);
                    Reg.r[opcodeArgs[0]] = Uint8(opcodeArgs[1]<<4|opcodeArgs[2]);
                    break;
                case 0x7:
                    //7XY0
                    printf(" Add %d to V%d(%d).", opcodeArgs[1]<<4|opcodeArgs[2], opcodeArgs[0], Reg.r[opcodeArgs[0]]);
                    Reg.r[opcodeArgs[0]] += Uint8(opcodeArgs[1]<<4|opcodeArgs[2]);
                    break;
                case 0x9:
                    //9XY0
                    if(Reg.r[opcodeArgs[0]] != Reg.r[opcodeArgs[1]])
                        Jump(&ptr, ptr + 4);
                    break;
                case 0x8:
                    switch(Opcode & 0x000F)
                    {
                    case 0x0:
                        //8XY0
                        printf(" Store V%d(%d) in V%d", opcodeArgs[1], Reg.r[opcodeArgs[1]], opcodeArgs[0]);
                        Reg.r[opcodeArgs[0]] = Reg.r[opcodeArgs[1]];
                        break;
                    case 0x1:
                        //8XY1
                        printf(" OR");
                        Reg.r[opcodeArgs[0]] = Uint8(Reg.r[opcodeArgs[1]] | Reg.r[opcodeArgs[0]]);
                        break;
                    case 0x2:
                        //8XY2
                        printf(" AND");
                        Reg.r[opcodeArgs[0]] = Reg.r[opcodeArgs[1]] & Reg.r[opcodeArgs[0]];
                        break;
                    case 0x3:
                        //8XY3
                        printf(" XOR");
                        Reg.r[opcodeArgs[0]] = Reg.r[opcodeArgs[1]] ^ Reg.r[opcodeArgs[0]];
                        break;
                    case 0x4:
                        //8XY4
                        printf(" Store V%d(%d) + V%d(%d) in V%d", opcodeArgs[1], Reg.r[opcodeArgs[1]], opcodeArgs[0], Reg.r[opcodeArgs[0]], opcodeArgs[0]);
                        Reg.r[0xF]=((int)(Reg.r[opcodeArgs[0]] + Reg.r[opcodeArgs[0]]) > 255) ? 1 : 0;
                        Reg.r[opcodeArgs[0]] += Reg.r[opcodeArgs[1]];
                        break;
                    case 0x5:
                        //8XY5
                        printf(" Store V%d(%d) - V%d(%d) in V%d", opcodeArgs[1], Reg.r[opcodeArgs[1]], opcodeArgs[0], Reg.r[opcodeArgs[0]], opcodeArgs[0]);
                        Reg.r[0xF] = (Reg.r[opcodeArgs[1]] > Reg.r[opcodeArgs[0]]) ? 1 : 0;
                        Reg.r[opcodeArgs[0]] -= Reg.r[opcodeArgs[1]];
                        break;
                    case 0x6:
                        //8XY6
                        printf(" VX >> 1");
                        Reg.r[0xF] = Reg.r[opcodeArgs[1]] & 0b1;
                        Reg.r[opcodeArgs[0]] = Reg.r[opcodeArgs[0]]>>1;
                        break;
                    case 0x7:
                        //8XY7
                        Reg.r[0xF] = (Reg.r[opcodeArgs[1]] > Reg.r[opcodeArgs[0]]) ? 1 : 0;
                        Reg.r[opcodeArgs[0]] = Reg.r[opcodeArgs[1]] - Reg.r[opcodeArgs[0]];
                        break;
                    case 0xE:
                        //8XYE
                        Reg.r[0xF] = Reg.r[opcodeArgs[0]]>>7;
                        Reg.r[opcodeArgs[0]] = Reg.r[opcodeArgs[0]]<<1;
                    default:
                        printf(" Unrecognized pattern.");
                        break;
                    }
                    break;
                case 0xA:
                    //ANNN
                    Reg.I = Uint16(opcodeArgs[0]<<8 | opcodeArgs[1]<<4 | opcodeArgs[2]);
                    printf(" Store %d in I", opcodeArgs[0]<<8 | opcodeArgs[1]<<4 | opcodeArgs[2]);
                    break;
                case 0xB:
                    printf(" Jump to %d+%d.", (opcodeArgs[0]<<8)|(opcodeArgs[1]<<4)|opcodeArgs[2], Reg.r[0]);
                    Jump(&ptr, opcodeArgs[0]<<8|opcodeArgs[1]<<4|opcodeArgs[2]+Reg.r[0]);
                    break;
                case 0xC:
                    //CXNN
                    Reg.r[opcodeArgs[0]] = Uint16(int(rand()%256)&(opcodeArgs[1]<<4|opcodeArgs[2]));
                    printf(" Random: V%d = %d", opcodeArgs[0], Reg.r[opcodeArgs[0]]);
                    break;
                case 0xD:
                {
                    //DXYN
                    int Ox = Reg.r[opcodeArgs[0]];
                    int Oy = Reg.r[opcodeArgs[1]];

                    int pUnset(0);
                    Reg.r[0xF] = 0;

                    for(int y1 = 0; y1 < opcodeArgs[2]; y1++)
                    {
                        Uint8 sprite = Proc.memory[Reg.I + y1];

                        for(int x1 = 0; x1 < 8; x1++)
                        {
                            //Hex to bin: get the value of each pixel
                            Uint8 mask = 0b1;
                            mask = mask << (7 - x1);
                            int pixel = sprite & mask;
                            pixel = (pixel > 0) ? 1 : 0;

                            //Draw pixel (with XOR)
                            bool newColor = (int)pixel ^ GetPixelState(Ox + x1, Oy + y1);
                            pUnset += pixel && GetPixelState(Ox + x1, Oy + y1);
                            ChangePixel(ChipScreen, Ox + x1, Oy + y1, newColor);
                        }
                    }
                    Reg.r[0xF] = (pUnset > 0);
                    printf(" Draw sprite: %d, %d, with length: %d. Pixel erased ? %d I: %d", Ox, Oy, opcodeArgs[2], Reg.r[0xF], Reg.I);
                    break;
                }
                case 0xE:
                    if((Opcode & 0x00FF) == 0xA1)
                    {
                        //EXA1
                        printf(" Skip if %X is unpressed... (%X)", Reg.r[opcodeArgs[0]], Proc.inputs[Reg.r[opcodeArgs[0]]]);
                        if(Proc.inputs[Reg.r[opcodeArgs[0]]] == 0)
                            Jump(&ptr, ptr + 4);
                    }
                    else
                    {
                        //EX9E
                        printf(" Skip if %X is pressed... (%X)", Reg.r[opcodeArgs[0]], Proc.inputs[Reg.r[opcodeArgs[0]]]);
                        if(Proc.inputs[Reg.r[opcodeArgs[0]]] == 1)
                            Jump(&ptr, ptr + 4);
                    }
                    break;
                case 0xF:
                    switch (Opcode & 0x00FF)
                    {
                    case 0x07:
                        //FX07
                        printf(" Get Timer (%d)", Reg.DT);
                        Reg.r[opcodeArgs[0]] = Reg.DT;
                        break;
                    case 0x0A:
                        //FX0A
                        printf(" Wait for key press");
                        Proc.waitForKeyPress = true;
                        Proc.waitForKeyInReg = opcodeArgs[0];
                        break;
                    case 0x15:
                        //FX15
                        printf(" Set timer to %d", Reg.r[opcodeArgs[0]]);
                        Reg.DT = Reg.r[opcodeArgs[0]];
                        break;
                    case 0x18:
                        //FX18
                        printf(" Set sound timer to %d (not enable).", Reg.r[opcodeArgs[0]]);
                        Reg.ST = (int)Reg.r[opcodeArgs[0]];
                        break;
                    case 0x1E:
                        //FX1E
                        printf(" Add V%d(%d) to I(%d).", opcodeArgs[0], Reg.r[opcodeArgs[0]], Reg.I);
                        //Reg.r[0xF] = (Reg.I + Reg.r[opcodeArgs[0]] > 0xF) ? 1 : 0;
                        Reg.I += Reg.r[opcodeArgs[0]];
                        break;
                    case 0x29:
                    {
                        //FX29
                        Reg.I = Reg.r[opcodeArgs[0]] * 5;
                        printf(" Char V%d(%X) -> I: %d", opcodeArgs[0], Reg.r[opcodeArgs[0]], Reg.I);
                        break;
                    }
                    case 0x33:
                        //FX33
                        printf(" Store decimal value of %d.", Reg.r[opcodeArgs[0]]);
                        Proc.memory[Reg.I] = (Reg.r[opcodeArgs[0]] - Reg.r[opcodeArgs[0]] % 100) / 100;
                        Proc.memory[Reg.I + 1] = (((Reg.r[opcodeArgs[0]] - Reg.r[opcodeArgs[0]] % 10)/10) % 10);
                        Proc.memory[Reg.I + 2] = Reg.r[opcodeArgs[0]] - Proc.memory[Reg.I] * 100 - 10 * Proc.memory[Reg.I + 1];
                        break;
                    case 0x55:
                        //FX55
                        printf(" Map regs in memory at addr: %d.", Reg.I);
                        for(int i = 0; i <= opcodeArgs[0]; i++)
                        {
                            Proc.memory[Reg.I + i] = Reg.r[i];
                        }
                        Reg.I += opcodeArgs[0] + 1;
                        break;
                    case 0x65:
                        //FX65
                        printf(" Map memory in regs from addr: %d.", Reg.I);
                        for(int i = 0; i <= opcodeArgs[0]; i++)
                        {
                            Reg.r[i] = Proc.memory[Reg.I + i];
                        }
                        Reg.I += opcodeArgs[0] + 1;
                        break;
                    default:
                        printf(" Unrecognized pattern.");
                        break;
                    }
                break;
                default:
                    printf(" Unrecognized pattern.");
                    break;
                }
            }
            ptr += 2;
            printf("\n");
            Time = SDL_GetTicks();
        }

        //Screen
        if(SDL_GetTicks()>=LastUpdate + 1000 / FPS)
        {
            SDL_Flip(ChipScreen);
            LastUpdate = SDL_GetTicks();
        }

        Timer(&Reg.DT, &Reg.ST);//Decrease timers

    }
    SDL_FreeSurface(ChipScreen);

    // all is well ;)
    printf("Exited cleanly\n");
    return 0;
}



void Jump(int* ptr, int addr)
{
    *ptr = addr-2;
}

void Init(CPU *proc, Registers *Reg)
{

    srand(time(NULL)); //Init rand

    (*proc).CharOffset = 0;
    for(int i=0; i<16; i++) (*proc).Subroutines[i] =- 1;
    (*proc).waitForKeyInReg = -1;
    (*proc).waitForKeyPress = false;
    for(int in=0; in<16;in++) (*proc).inputs[in] = 0;// Initialize keys

    for(int reg=0; reg<18; reg++) (*Reg).r[reg] = 0; //Initialize registers
    (*Reg).I = 0;
    (*Reg).DT = 0;
    (*Reg).ST = 0;

    for(int mem=0; mem<4096; mem++) (*proc).memory[mem] = 0; //Initialize processor memory

    //Load characters
    char t[161] = "F0909090F02060202070F010F080F0F010F010F09090F01010F080F010F0F080F090F0F010204040F090F090F0F090F010F0F090F09090E090E090E0F0808080F0E0909090E0F080F080F0F080F08080";
    for(int mem = 0; mem < 80; mem++)
    {
        if(t[mem*2] > '9')
        {
            (*proc).memory[mem] = Uint8(t[mem*2] - 'A' + 0xA)<<4;
        }
        else
        {
            (*proc).memory[mem] = Uint8(t[mem*2] - '0')<<4;
        }
    }
}
void ChangePixel(SDL_Surface *surface, int x, int y, bool color)
{
    for(int x1 = 0; x1< PIXEL_SIZE; x1++)
    {
        for(int y1 = 0; y1< PIXEL_SIZE; y1++)
        {
            *((Uint32*)(surface->pixels) + (x % X_RESOL) * PIXEL_SIZE + x1 + ((y % Y_RESOL) * PIXEL_SIZE + y1) * surface->w) = SDL_MapRGB(surface->format, 255, 255, 255) * color;
        }
    }
    Screen[x % X_RESOL + (y % Y_RESOL) * X_RESOL] = color;
}

bool GetPixelState(int x, int y)
{
    return Screen[x % X_RESOL + (y % Y_RESOL) * X_RESOL];
}
void ClearScreen(SDL_Surface *screen)
{
    for(int x = 0; x < X_RESOL; x++)
    {
         for(int y = 0; y < Y_RESOL; y++)
        {
            ChangePixel(screen, x, y, false);
        }
    }
}
int OpenGame(char *path, CPU* proc)
{
    printf("Open file: %s\n",path);
    FILE *file = fopen(path, "rb");
    if(!file) return 9999;

    fseek(file, 0, SEEK_END);
    int fsize = ftell(file);
    printf("File length: %d\n", fsize);

    fseek(file, 0, SEEK_SET);
    fread((*proc).memory+0x200, sizeof(Uint8), fsize, file);
    printf("File content: ");
    for(int i =0; i<4096; i++) printf("%X", (*proc).memory[i]);
    printf("\n");
    fclose(file);

    //Set window title
    string filepath(path);
    size_t ext(filepath.find_first_of("."));
    filepath = filepath.substr(1, ext-1);

    size_t specchar(filepath.find_first_of("["));
    while(specchar != string::npos)
    {
        filepath.replace(specchar, 1, "(");
        specchar = filepath.find_first_of("[");
    }

    specchar = filepath.find_first_of("]");
    while(specchar != string::npos)
    {
        filepath.replace(specchar, 1, ")");
        specchar = filepath.find_first_of("]");
    }

    specchar = filepath.find_first_of("-");
    while(specchar != string::npos)
    {
        filepath.replace(specchar, 1, " ");
        specchar = filepath.find_first_of("-");
    }

    int namepos(filepath.find_last_of("/\\"));
    const char *filename(filepath.substr(namepos + 1).c_str());
    SDL_WM_SetCaption(filename, filename);

    return fsize;
}

bool InitSDL(SDL_Surface** screen)
{
    // initialize SDL video
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return 1;
    }

    // make sure SDL cleans up before exit
    atexit(SDL_Quit);

    (*screen) = SDL_SetVideoMode(X_RESOL * PIXEL_SIZE, Y_RESOL * PIXEL_SIZE, 32, SDL_HWSURFACE|SDL_DOUBLEBUF);
    if ( !screen )
    {
        printf("Unable to set %dx%d video: %s\n", X_RESOL * PIXEL_SIZE, Y_RESOL * PIXEL_SIZE, SDL_GetError());
        return 1;
    }
    return 0;
}
void Timer(int *DT, int *ST)
{
    static int DecTimers(0);
    if(SDL_GetTicks()>=DecTimers + 1000 / 60)
    {
        (*DT) = ((*DT) > 0) ? ((*DT) - 1) : 0;
        (*ST) = ((*ST) > 0) ? ((*ST) - 1) : 0;
        DecTimers = SDL_GetTicks();
    }
    //Update
}
