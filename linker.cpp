#include <iostream>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <map>
#include <vector>
#include <iomanip>
#include <ios>

//declaring a variable to hold inputfil that should given as command line arg
std::ifstream input;
//helper variable for getToken
char line[4096];
static char* nextToken = nullptr;
//struct to represent a symbol that has a name, value(relative +absolute), absolute(module num defined in)
//abs_val=the value in that module(not including relative), used(if it was used in E instruction), defined(if it was already defined)
struct Symbol {
    const char* name;
    int value;
    int absolute;
    int abs_val;
    int used = false;
    bool defined=false;
};
//the struct below is used only in pass 2 so it does not store anything between pass 1 and 2
//used to differentiate between symbols of uselist and symbols that were actually defined
struct Usedsymbol
{
    std::string name;
    bool used=false;
};
//simple struct to represetn a module that contains its relative address
struct Module{
    int relative_address;
};
int len_last_token=0;
//data structure that represents a symbol table and module table respectively
std::map<std::string, Symbol> symbolTable;
std:: map< int, Module> moduleTable;
//from the lab instructions
void __parseerror(int errcode, int linenum, int lineoffset) {
    static const char* errstr[] = {
            "TOO_MANY_DEF_IN_MODULE", // > 16
            "TOO_MANY_USE_IN_MODULE", // > 16
            "TOO_MANY_INSTR",         // total num_instr exceeds memory size (512)
            "NUM_EXPECTED",           // Number expected
            "SYM_EXPECTED",           // Symbol Expected
            "MARIE_EXPECTED",          // Addressing Mode Expected (M/A/R/I/E)
            "SYM_TOO_LONG",           // Symbol too long
    };
    std:: cout << "Parse Error line " << linenum << " offset " << lineoffset << ": " << errstr[errcode] << std::endl;
    exit(1);
}
//gets tokens one by one
const char* get_token(int& lineNumber, int& lineOffset) {
    //loops until it finds a valid token
    while (true) {
        //if token is null
        if (nextToken == nullptr) {
            //if end of file is reached update variables and return null
            //I wasn't sure how to implement it without using getline
            //but I was still able to achieve the correct functionality
            if (!input.getline(line, sizeof(line))) {
                lineOffset+=len_last_token;
                return nullptr; // No more tokens (end of file)
            }
            //else end of file has not been reached so we update variables and get next valid token
            lineNumber++; // updates line number
            nextToken = strtok(line, " \t\n"); // Get the first token
            // if no tokens in that line go to the next
            if (nextToken == nullptr) {
                continue; // skip the line and read the next one
            }
            lineOffset = nextToken - line + 1; // update for the new line
        } else {
            lineOffset = nextToken - line + 1; // update for the current token
        }

        const char* token = nextToken; // retrieves current token
        // gets the next token for the next call
        nextToken = strtok(nullptr, " \t\n");
        //gets the size of token to update line offset for the next token  if parse error occurs
        len_last_token=strlen(token);
        return token; // Return the current token
    }
}
//converts the token to an int and returns the int
int readInt(const char* token,int lineNum,int lineOffset)
{
    //if token is null we have a parse error
    if(token == nullptr)
    {
        __parseerror(3, lineNum, lineOffset);  // NUM_EXPECTED
        return -1;
    }
    const char * temp=token;
    //if token is negative parse error
    if(*temp=='-')
    {
        __parseerror(3, lineNum, lineOffset);  // NUM_EXPECTED
        return -1;
    }
    //check that token is an int if not we have a parse error
    for(; *temp!='\0'; temp++) {
        if (std::isdigit(*temp)) {
            continue;
        } else {
            __parseerror(3, lineNum, lineOffset);  // NUM_EXPECTED
            return -1;

        }
    }
    //try-catch was included here just in case even though if the program got to this point
    //we know that the token is definitely an integer
    try {
        //if is an int we convert the string token to an int and return it
        return std::stoi(token);

    } catch (const std::invalid_argument& e) {
        __parseerror(3, lineNum, lineOffset);
        return -1;
    } catch (const std::out_of_range& e) {
        __parseerror(3, lineNum, lineOffset);
        return -1;
    }
}
//checks if the token is a valid symbol according to the requirements
bool isValidSymbol(const char * sym, int lineNumber, int lineOffset) {
    //checks if null first
    if (sym == nullptr)
    {
        __parseerror(4, lineNumber, lineOffset);  // SYM_EXPECTED
    }
    //checks length of symbol first
    if (strlen(sym) > 16) {
        __parseerror(6,lineNumber,lineOffset);// SYM_TOO_LONG
        return false;
    }
    //checks if first character is a alphabetical character
    if (!isalpha(sym[0])) {
        __parseerror(4,lineNumber,lineOffset); // SYM_EXPECTED
        return false;
    }
    //checks if remaining characters are alphanumerical characters
    for (const char* temp=sym; *temp!='\0'; temp++) {
        if (!isalnum(*temp)) {
            __parseerror(4,lineNumber,lineOffset);// SYM_EXPECTED
            return false;
        }
    }
    return true;
}
//checks if token passed is a valid address instruction mode
char is_marie(const char* token,int lineNum, int lineOffset) {
    //if null or multiple character we know its invalid
    if(token == nullptr || std::strlen(token)>1)
    {
        __parseerror(5,lineNum,lineOffset);  // ADDR_EXPECTED
        return '\0';
    }

    char addrMode = token[0];
    //checks against valid address instruction modes
    if (addrMode != 'A' && addrMode != 'R' && addrMode != 'E' && addrMode != 'I' && addrMode != 'M') {
        __parseerror(5,lineNum,lineOffset);  // ADDR_EXPECTED
        return '\0';
    }
    return addrMode;
}
//tried to resemble as much as I can with the pseudocode in the lab1 instructions
//this is pass1
void pass1(int argc, char* argv[])
{
    //try catch here is for dealing with the file
    try {
        //opens file
        input.open(argv[1]);
        if (!input) {
            std:: cout<<"Error: Unable to open input file";
        }
        //intitialize helper variables
        const char* token;
        int lineNumber = 0;
        int lineOffset = 0;
        int instructions=0;
        int module_num=-1;
        // gets token one at a time starting with the def count
        while (true) {
            //get defcount and check if null
            token = get_token(lineNumber, lineOffset);
            if (token == nullptr) break ; // Exit if no more tokens
            //check the defcount
            int defCount=readInt(token,lineNumber,lineOffset);
            //if valid increment module # since we are in a new module
            module_num=module_num+1;
            //checks defcount constraint
            if(defCount>16)
            {
                __parseerror(0,lineNumber,lineOffset);//TOO_MANY_DEF
            }
            //loops until all defintion pairs have been gone through
            //gets the symbol then the value of the resepctive symbol and stores into symbol table
            for(int i=0;i<defCount;i++)
            {
                //gets symbol
                token = get_token(lineNumber, lineOffset);
                //checks if valid symbol
                if(isValidSymbol(token, lineNumber,lineOffset))
                {
                    //if so create a key for the map of the symbol table
                    std::string symbol_name(token);
                    //checks if key is already present which means it is already defined
                    if(symbolTable.count(symbol_name)>0)
                    {
                        //even if symbol was already defined we still have to read its associated value so we can move on to next symbol
                        token = get_token(lineNumber, lineOffset);
                        //sets defined to true which indicates it has been attempted to redefine
                        symbolTable[symbol_name].defined=true;
                        //prints warning according to rule 5
                        std:: cout<<"Warning: Module " << module_num << ": " << symbol_name << " redefinition ignored"<<'\n';
                        continue;
                    }else
                    {
                        //if symbol has not been defined create a new symbol and store it in the symbol table
                        Symbol symbol;
                        symbol.name = token;
                        token = get_token(lineNumber, lineOffset);
                        int value = readInt(token, lineNumber, lineOffset);
                        symbol.value = value + instructions;
                        symbol.abs_val=value;
                        symbol.absolute=module_num;//for rule 4
                        symbolTable[symbol_name] = symbol;
                    }
                }
            }
            //now we are done with definition list so time to move onto uselist

            token = get_token(lineNumber, lineOffset);
            //gets use count and checks if valid int
            int useCount= readInt(token,lineNumber,lineOffset);
            //checks against usecount constraint
            if (useCount>16)
            {
                __parseerror(1,lineNumber,lineOffset);//TOO_MANY_USE
            }
            //loop through all the used symbols
            for (int i=0;i<useCount;i++)
            {
                //gets each used symbol
                token = get_token(lineNumber, lineOffset);
                //checks if its a valid symbol or null
                //if valid we don't need to do anything with it in pass 1
                if(token==nullptr)
                {
                    __parseerror(4,lineNumber,lineOffset);//SYM_EXPECTED
                }
                if(isValidSymbol(token, lineNumber,lineOffset))
                {
                    continue;
                }
            }
            //now we are done with the use list so now we got to address list
            token = get_token(lineNumber, lineOffset);
            //gets # of address instructions
            int num_instructions= readInt(token,lineNumber,lineOffset);
            //checks if cumulative address instructions has violated constraints
            if(instructions+num_instructions>512)
            {
                __parseerror(2,lineNumber,lineOffset);// TOO_MANY_INSTR

            }
            //loop through instructions
            for(int i=0;i<num_instructions;i++)
            {
                //get the instruction mode and check if valid
                token = get_token(lineNumber, lineOffset);
                if(is_marie(token,lineNumber,lineOffset))
                {
                    //if valid get the instruction integer and check if valid
                    token = get_token(lineNumber, lineOffset);
                    int num = readInt(token, lineNumber, lineOffset);
                }
            }
            //checks if symbol values are greater than module sizes and if so prints a warning according to rule 5
            for( auto& pair : symbolTable)
            {
                const std::string& key=pair.first;
                Symbol &symbol=pair.second;
                if(symbol.absolute!=module_num)
                {
                    continue;
                }
                else
                {
                    if(symbol.abs_val>num_instructions)
                    {
                        std:: cout<<"Warning: Module " << module_num <<": " << key << "=" << symbol.abs_val <<" valid=[0.."<< num_instructions-1<<"] assume zero relative"<<'\n';
                        symbol.value= instructions;
                    }
                }
            }
            //creates module table
            Module module;
            module.relative_address=instructions;
            moduleTable[module_num]=module;
            //adds to cumulative instructions we have seen so far
            instructions+=num_instructions;
        }
        //prints symbol table
        std:: cout<<"Symbol Table\n";
        for(const auto& pair: symbolTable)
        {
            const std::string& key=pair.first;
            const Symbol& symbol=pair.second;
            std:: cout<<key<<"="<< symbol.value;
            //check to see if it was attempted to redfine symbol
            if(symbol.defined)
            {
                //rule 2
                std:: cout<<" Error: This variable is multiple times defined; first value used"<<'\n';
            }else
            {
                std:: cout<<'\n';
            }

        }
        //closes file
        input.close();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
//pass 2 very similar to pass 1 but with a few changes as described in the lab instructions
//I won't be commenting things that are identical to pass 1 mainly the changes for this method
void pass2(int argc, char* argv[])
{
    try {
        input.open(argv[1]);
        if (!input) {
            std:: cout<<"Error: Unable to open input file";
        }
        const char* token;
        int lineNumber = 0;
        int lineOffset = 0;
        int instructions=0;
        int module_num=-1;
        //print for the memory map
        std:: cout<<"\nMemory Map\n";
        //same loop as before
        while (true) {
            token = get_token(lineNumber, lineOffset);
            if (token == nullptr) break ; // Exit if no more tokens
            int defCount=readInt(token,lineNumber,lineOffset);
            module_num=module_num+1;
            if(defCount>16)
            {
                __parseerror(0,lineNumber,lineOffset);
            }
            //now when we loop through symbol definitions no need to store but I still am doing checks
            //just to ensure input is correct even though I believe technically this is not needed
            //so here we just get through the tokens and don't really do anything with them beside the checks
            for(int i=0;i<defCount;i++)
            {
                token = get_token(lineNumber, lineOffset);
                if(isValidSymbol(token, lineNumber,lineOffset))
                {
                    token = get_token(lineNumber, lineOffset);
                    int value = readInt(token, lineNumber, lineOffset);

                }
            }
            token = get_token(lineNumber, lineOffset);
            int useCount= readInt(token,lineNumber,lineOffset);
            if (useCount>16)
            {
                __parseerror(1,lineNumber,lineOffset);
            }
            //now here for the input uselist we actually create a uselist for each module of type UsedSymbol
            //which was a struct that I created above. So this proves that UsedSymbol struct was not created in pass1
            //and stored for pass 2 it was just created and used in pass 2
            //regardless this essentially holds the symbols of the modules that are in the uselist input
            //still doing error checks as mentioned above just in case
            std::vector<Usedsymbol> useList(useCount);
            for (int i=0;i<useCount;i++)
            {
                token = get_token(lineNumber, lineOffset);
                if(token==nullptr)
                {
                    __parseerror(4,lineNumber,lineOffset);
                }
                if(isValidSymbol(token, lineNumber,lineOffset))
                {
                    Usedsymbol symbol;
                    symbol.name=std::string (token);
                    useList[i]=symbol;
                    continue;
                }
            }

            token = get_token(lineNumber, lineOffset);
            int num_instructions= readInt(token,lineNumber,lineOffset);

            if(instructions+num_instructions>512)
            {
                __parseerror(2,lineNumber,lineOffset);

            }
            //now when looping through instructions we actually execute the instruction
            for(int i=0;i<num_instructions;i++)
            {
                token = get_token(lineNumber, lineOffset);
                if(is_marie(token,lineNumber,lineOffset)) {
                    //get the address mode
                    char marie = token[0];
                    token = get_token(lineNumber, lineOffset);
                    //get instruction integer
                    int num = readInt(token, lineNumber, lineOffset);
                    //get the opcode and operand according to definitions from lab1
                    int opcode = num / 1000;
                    int operand = num % 1000;
                    //do opcode constraint check since if invalid we can just continue to the next instruction
                    if (opcode >= 10) {
                        //rule 11
                        std::cout << std::setfill('0') << std::setw(3);
                        std::cout << i + moduleTable[module_num].relative_address
                                  << ": 9999 Error: Illegal opcode; treated as 9999" << '\n';
                        continue;
                    }
                    //if opcode valid we execute the instruction
                    switch(marie)
                    {
                        //here we first check if the operand is > than the amount of modules using an iterator
                        //which is where our module table comes into play
                        case 'M':
                            //ensures module table is not empty
                            if(!moduleTable.empty())
                            {
                                //gets the final module which will have the largest module # implicitly
                                auto greatestModule = std::prev(moduleTable.end());
                                int greatestModuleNum = greatestModule->first;
                                //checks if oeprand is larger than this module # if so we have an error
                                if(operand > greatestModuleNum )
                                {
                                    //rule 12
                                    std::cout << std::setfill('0') << std::setw(3);
                                    std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                    std::cout <<std::setfill('0')<<std::setw(4)<< num-operand <<" Error: Illegal module operand ; treated as module=0" << '\n';
                                }
                                else
                                {
                                    //we have a valid operand for this address instructions so execute functionality
                                    //here we just add the module's relative address to the instruction integer base
                                    std::cout << std::setfill('0') << std::setw(3);
                                    std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                    std::cout<< std::setfill('0')<<std::setw(4);
                                    std::cout<<moduleTable[operand].relative_address+ num-operand<<'\n';
                                }
                                break;
                            }
                        case 'A':
                            //checks if valid operand for A address instruction mode
                            if (operand>512)
                            {
                                //rule 8
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout <<std::setfill('0')<<std::setw(4)<< num-operand <<" Error: Absolute address exceeds machine size; zero used" << '\n';
                            }
                            else
                            {
                                //executes the functionality by simply just putting the instruction itneger as output
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout<< std::setfill('0')<<std::setw(4);
                                std::cout<<num<<'\n';
                            }
                            break;
                        case 'R':
                            //checks if operand is greater than the relative module size and if so error
                            if (operand>num_instructions)
                            {
                                //rule 9
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout << std::setfill('0')<<std::setw(4)<<num-operand+moduleTable[module_num].relative_address <<" Error: Relative address exceeds module size; relative zero used" << '\n';
                            }
                            else
                            {
                                //if valid operand we just add the instruction integer to the module_num
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout<< std::setfill('0')<<std::setw(4);
                                std::cout<<num+moduleTable[module_num].relative_address<<'\n';
                            }
                            break;
                        case 'I':
                            //checks operand against constraint if so we use 999
                            if(operand >= 900)
                            {
                                //rule 10
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout <<std::setfill('0')<<std::setw(4) << num-operand + 999 <<" Error: Illegal immediate operand; treated as 999" << '\n';
                            }
                            else
                            {
                                //if valid we just put the instruction integer directly
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout<< std::setfill('0')<<std::setw(4);
                                std::cout<<num<<'\n';
                            }
                            break;
                        case 'E':
                            //checks if operand is greater than size of use list if so invalid
                            if(operand>=useCount)
                            {
                                //rule 6
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout<<std::setfill('0')<<std::setw(4)<< num-operand+moduleTable[module_num].relative_address<<" Error: External operand exceeds length of uselist; treated as relative=0" << '\n';
                            }
                                //checks if symbol that wants to be used is defined if not invalid
                            else if(symbolTable.count(useList[operand].name)==0)
                            {
                                //rule 3
                                useList[operand].used=true;
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout<<std::setfill('0')<< std::setw(4)<< num-operand<<" Error: "<< useList[operand].name <<" is not defined; zero used" << '\n';
                            }
                                //valid operand so execute instruction
                            else
                            {
                                //indicate uselist symbol was used for rule 7
                                useList[operand].used=true;
                                //indicate symbol table symbol was used for rule 4
                                symbolTable[useList[operand].name].used=true;
                                Symbol symbol_used=symbolTable[useList[operand].name];
                                std::cout << std::setfill('0') << std::setw(3);
                                std::cout << i + moduleTable[module_num].relative_address<< ": ";
                                std::cout<< std::setfill('0')<<std::setw(4);
                                std::cout<<num-operand+symbol_used.value<<'\n';
                            }
                            break;

                    }

                }

            }
            //does check for rule 7
            for(int i=0;i<useCount;i++)
            {
                if(useList[i].used)
                {

                    continue;
                }
                else
                {
                    std:: cout<<"Warning: Module "<<module_num <<": "<< "uselist["<<i<<"]="<<useList[i].name<< " was not used"<<'\n';
                }

            }
            instructions+=num_instructions;
        }
        std:: cout<<'\n';
        //check for rule 4
        for( const auto& pair : symbolTable)
        {
            const std::string& key = pair.first;
            const Symbol& symbol = pair.second;
            if (symbol.used)
            {
                continue;
            }
            else
            {
                std:: cout<<"Warning: Module "<< symbol.absolute <<": " << key << " was defined but never used\n";

            }
        }

        input.close();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std:: cout << "Usage: " << argv[0] << " <input-file>" << std::endl;
        return 1;
    }
    pass1(argc,argv);
    pass2(argc, argv);

    return 0;
}






