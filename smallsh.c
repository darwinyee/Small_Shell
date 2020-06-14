/*******************************************************************************
** Author:       Darwin Yee, ONID 933915366
** Date:         11-11-2019
** Project 3:    Smallsh
** Description:  This is a custom shell written in C which can perform some basic
                 shell commands.
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 2048
#define MAX_ARGU 512

//for parent pid
pid_t parentPid;

//for foreground pid;
pid_t foregroundPid;

//for background completed messages
char* bkMessage = NULL;
int bkCap = 10;

//Enable foreground only mode
int foregroundOnly = 0;

//Command struct
struct Command{
	char* command;
	char** arguments;
	char* input;
	char* output;
	char* background;
	char* originalCommand;
	int argumentCt;
	int needInput;
	int needOutput;
	int isBkgd;
};


//This function converts integer to string, it will not check if the buffersize is bigger than the number	
int IntegerToString(int target, char* buffer, int bufferSize){
	
	//reset buffer
	memset(buffer, '\0', bufferSize);
	
	char* intList = "0123456789";
	int curIdx = 0;
	
	//check if the number is negative
	if(target < 0){
		buffer[curIdx] = '-';
		curIdx++;
	}
	
	//count the number of digits
	int numOfDigits = 1;
	int targetCp = target;
	while(targetCp > 9){
		targetCp = (targetCp-targetCp%10)/10;
		numOfDigits++;
	}
	
	//array to store the digit
	int* digits = (int*)malloc(sizeof(int)*numOfDigits);
	int i;
	for(i = 0; i < numOfDigits; i++){
		digits[i] = target%10;
		target = (target-target%10)/10;
	}
	
	//store to buffer
	for(i = numOfDigits-1; i >= 0; i--){
		if(curIdx < bufferSize-1){
			buffer[curIdx] = intList[digits[i]];
			curIdx++;
		}
	}
	
	//free memory
	free(digits);

	return numOfDigits;
	
}

//This function expands $$ to pid
char* ExpandPid(char* target, int wordLength){
	
	//get the pid and convert to string
	char* strPid = malloc(sizeof(char)*10);
	memset(strPid, '\0', 10);	
	int digitCt2 = IntegerToString(getpid(), strPid, 10);
	
	//create a buffer that is enough
	char* temp = malloc(sizeof(char)*(wordLength/2+1)*digitCt2);
	memset(temp, '\0',(wordLength/2+1)*digitCt2);
	
	//start copying
	int idx = 0;
	int resultIdx = 0;
	
	//if '$' is encountered and if the next character is '$', copy the PID, if not, copy the current character
	while((idx < wordLength) && (target[idx] != '\0')){
		if(target[idx] == '$'){
			if(target[idx+1] == '$'){
				int i;
				for(i = 0; i < digitCt2; i++){
					temp[resultIdx] = strPid[i];
					resultIdx++;
				}
				idx = idx + 2;
			}else{
				temp[resultIdx] = target[idx];
				resultIdx++;
				idx++;
			}
		}else{
			temp[resultIdx] = target[idx];
			resultIdx++;
			idx++;
		}
		
	}

	//free memory
	free(strPid);
	free(target);
	
	return temp;
}

//This function initializes the command structure
void InitializeCommand(struct Command* thisCommand, char* originalCommand, char* firstCommand){
	//command memory
	thisCommand->command = malloc(sizeof(char)*MAX_COMMAND_LENGTH);
	memset(thisCommand->command, '\0', MAX_COMMAND_LENGTH);
	strcpy(thisCommand->command, firstCommand);
	
	//argument array
	thisCommand->arguments = (char**)malloc(sizeof(char*)*MAX_ARGU);
	int i;
	for(i = 0; i < MAX_ARGU; i++){
		thisCommand->arguments[i] = NULL;
	}
	
	//input memory
	thisCommand->input = malloc(sizeof(char)*MAX_COMMAND_LENGTH);
	memset(thisCommand->input, '\0', MAX_COMMAND_LENGTH);
	
	//output memory
	thisCommand->output = malloc(sizeof(char)*MAX_COMMAND_LENGTH);
	memset(thisCommand->output, '\0', MAX_COMMAND_LENGTH);
	
	//background memory
	thisCommand->background = malloc(sizeof(char)*2);
	memset(thisCommand->background, '\0', 2);
	
	//original Command
	thisCommand->originalCommand = malloc(sizeof(char)*MAX_COMMAND_LENGTH);
	memset(thisCommand->originalCommand, '\0', MAX_COMMAND_LENGTH);
	strcpy(thisCommand->originalCommand, originalCommand);
	
	thisCommand->argumentCt = 0;
	thisCommand->needInput = 0;
	thisCommand->needOutput = 0;
	thisCommand->isBkgd = 0;
	
}

//This function splits the command into word and stores to array. The input must not end with newline.
char** SplitLineBySpace(char* inLine, int* wordCtPt){
	//count the number of words
	int charCt = 0;
	int wordCt = 0;
	int newWord = 0;
	while(inLine[charCt] != '\0'){
		if(inLine[charCt] != ' ' && newWord == 0){
			newWord = 1;
			wordCt++;
		}
		if(inLine[charCt] == ' ' && newWord == 1){
			newWord = 0;
		}
		charCt++;
	}

	//Allocate memory for word array
	*wordCtPt = wordCt;
	if(wordCt > 0){
		char** result = (char**)malloc(sizeof(char*)*wordCt);
		assert(result != NULL);
		int i;
		for(i = 0; i < wordCt; i++){
			result[i] = NULL;
		}
		charCt = 0; newWord = 0; 
		int wordStartIdx = 0;
		int curWordIdx = 0;
		while(inLine[charCt] != '\0'){
			if(inLine[charCt] != ' ' && newWord == 0){   //discover new word
				newWord = 1;
				wordStartIdx = charCt;
			}
			if(inLine[charCt] == ' ' && newWord == 1){   //that is the end of this word, copy this word to a new char variable and store it to result array
				int wordLength = charCt-wordStartIdx+1;
				char* curWord = malloc(sizeof(char)*(wordLength));
				assert(curWord);
				memset(curWord, '\0', wordLength);
				int i;
				for(i = 0; i < wordLength-1; i++){
					curWord[i] = inLine[i+wordStartIdx];
				}
				
				newWord = 0;
				
				//expand $$ to pid
				curWord = ExpandPid(curWord, wordLength);
				
				result[curWordIdx] = curWord;
				curWordIdx++;
			}
			charCt++;
		}
		//store the last word to the result array
		if(newWord == 1){
			int finalLen = charCt-wordStartIdx+1;
			char* finalWord = malloc(sizeof(char)*(finalLen));
			assert(finalWord);
			memset(finalWord, '\0', finalLen);
			int i;
			for(i = 0; i < finalLen-1; i++){
				finalWord[i] = inLine[i+wordStartIdx];
			}
			newWord = 0;
			
			//expand $$ to pid
			finalWord = ExpandPid(finalWord, finalLen);
			
			result[curWordIdx] = finalWord;
			curWordIdx++;
		}
		
		return result;
	}
	
	return NULL;
}

//This function extracts different parts of the command and stores to command struct
struct Command* BuildCommand(char* inLine, int lineCharCt){
	
	//remove the last \n from inLine
	if(lineCharCt < 1)
		return NULL;
	else
		inLine[lineCharCt-1] = '\0';
	
	//read the input command and store each part to array of words
	int wordCt = 0;
	char** temp = SplitLineBySpace(inLine, &wordCt);
	
	//identify each part of the command
	if(wordCt > 0){
		
	    struct Command* thisCommand = (struct Command*)malloc(sizeof(struct Command));
	    assert(thisCommand != NULL);
		
		//initialize the structure
		InitializeCommand(thisCommand, inLine, temp[0]);
		
		//loop to count the number of arguments and get the input and output redirection and bg/fg
		int lastArgIdx = 0;
		int i;
		for(i = 1; i < wordCt; i++){
			if(strcmp(temp[i], "<") == 0){
				strcpy(thisCommand->input, temp[i+1]);
				thisCommand->needInput = 1;
				i++;			
			}else if(strcmp(temp[i], ">") == 0){		
				strcpy(thisCommand->output, temp[i+1]);
				thisCommand->needOutput = 1;
				i++;			
			}else if(lastArgIdx == i-1){
				lastArgIdx++;
			}
		}
		if(strcmp(temp[wordCt-1], "&") == 0){
			strcpy(thisCommand->background, temp[wordCt-1]);
			thisCommand->isBkgd = 1;
			if(thisCommand->needInput == 0 && thisCommand->needOutput == 0)
				lastArgIdx--;
		}
		
		//put all arguments into array, store the first command twice for exec function
		thisCommand->argumentCt = lastArgIdx+1;
		for(i = 0; i < thisCommand->argumentCt; i++){
			thisCommand->arguments[i] = malloc(sizeof(char)*MAX_COMMAND_LENGTH);
			memset(thisCommand->arguments[i], '\0', MAX_COMMAND_LENGTH);
			strcpy(thisCommand->arguments[i], temp[i]);
		}
		
		//free the char** temp
		for(i = 0; i < wordCt; i++){
			free(temp[i]);
		}
		free(temp);
		
		//return the structure pointer
		return thisCommand;
	}
	
	return NULL;
	
}

//This function frees the struct command
void FreeCommandStruct(struct Command* target){
	if(target != NULL){
		free(target->command);
		free(target->input);
		free(target->output);
		free(target->background);
		free(target->originalCommand);
		int temp = 0;
		while(target->arguments[temp] != NULL){
			free(target->arguments[temp]);
			temp++;
		}
		free(target->arguments);
	}
}

//This function prints the command information on screen.
void PrintCommandInfo(struct Command* target){
	printf("New Command Info:\n");
	printf("command: %s\n", target->command);
	printf("input: %s\n", target->input);
	printf("output: %s\n", target->output);
	printf("background: %s\n", target->background);
	printf("Original: %s\n", target->originalCommand);
	printf("argument ct: %d\n", target->argumentCt);
	if(target->argumentCt > 0){
		printf("Arguments:");
		int i;
		for(i = 0; i < target->argumentCt; i++){
			printf(" %s", target->arguments[i]);
		}
		printf("\n");
	}
	printf("needInput: %d\n", target->needInput);
	printf("needOutput: %d\n", target->needOutput);
}

//This function saves background completed messages to bkMessage
//input message should include \n for better onscreen result
void SaveBackgroundMessage(char* message){
	//create buffer
	if(bkMessage == NULL){
		bkMessage = malloc(sizeof(char)*bkCap);
		memset(bkMessage, '\0', bkCap);
	}
	
	//find the starting idx to write
	int i; int startWriteIdx = 0;
	for(i = 0; i < bkCap; i++){
		if(bkMessage[i] == '\0'){
			startWriteIdx = i;
			break;
		}
	}
	
	//write the message
	int messageCt = 0;
	while(message[messageCt] != '\0'){
		bkMessage[startWriteIdx] = message[messageCt];
		startWriteIdx++;
		messageCt++;
		
		//resize array if needed
		if(startWriteIdx == bkCap){
			char* oldArr = bkMessage;
			bkMessage = malloc(sizeof(char)*bkCap*2);
			memset(bkMessage, '\0', bkCap*2);
			
			for(i = 0; i < bkCap; i++){
				bkMessage[i] = oldArr[i];
			}
			
			free(oldArr);
			bkCap = bkCap*2;
		}
	}
}

//Signal handling function

//This function changes the foreground/background mode after CTRL-Z is received
void catchSIGTSTP(int signo){
	if(parentPid == getpid()){
		if(foregroundOnly == 1){
			char* message = "\nExiting foreground-only mode\n: ";
			write(STDOUT_FILENO, message, 32);
			foregroundOnly = 0;
		}else{
			char* message = "\nEntering foreground-only mode (& is now ignored)\n: ";
			write(STDOUT_FILENO, message, 52);
			foregroundOnly = 1;
		}
		
	}
}

//This function retrieves the exit status of Child processes and save it to a variable "bkMessage".
void catchSIGCHLD(int signo){
	int childExitStatus = -5;
	pid_t childPid = waitpid(-1,&childExitStatus,WNOHANG);
	
	if(childPid != foregroundPid && childPid > 0){
		
		char message[100];
		memset(message, '\0', 100);
		SaveBackgroundMessage("background pid \0");
		int digitCt = IntegerToString(childPid, message, 100);
		SaveBackgroundMessage(message);
		SaveBackgroundMessage(" is done: \0");
		if(WIFEXITED(childExitStatus)){   //if the child is terminated normally
			int exitStatus = WEXITSTATUS(childExitStatus);
			SaveBackgroundMessage("exit value \0");	
			digitCt = IntegerToString(exitStatus, message, 100);   //convert integer to string and return the number of digits.
			message[digitCt] = '\n';
			SaveBackgroundMessage(message);		
		}                                 //if the child is terminated by signal
		else if(WIFSIGNALED(childExitStatus)){
			int termSignal = WTERMSIG(childExitStatus);
			SaveBackgroundMessage("terminated by signal \0");
			digitCt = IntegerToString(termSignal, message, 100);    //convert integer to string and return the number of digits.
			message[digitCt] = '\n';
			SaveBackgroundMessage(message);
		}
	}

}

//built-in shell functions
//This function terminates all child processes and exit the shell
int ExitShell(){
	//ignore child signal
	signal(SIGCHLD, SIG_IGN);
	
	//send SIGINT to kill child processes
	kill(-getpid(),SIGTERM);
	
	//wait for all process to terminates
	int childExitStatus = -5;
	
	while((int)wait(&childExitStatus) > 0){
	}
	
	return 1;
}

//This function changes the current working directory to the one specified.
void ChangeDir(char* targetDir){
	
	int chStatus = 0;
	
	//if the given argument is ".." or ".", call chdir on the argument.
	if(strcmp(targetDir, "..") == 0 || strcmp(targetDir, ".") == 0){
		chStatus = chdir(targetDir);
	}
	
	//if no argument is given, change to the system HOME directory.
	else if(strlen(targetDir) == 0){
		char* homeEnv = getenv("HOME");
		chStatus = chdir(homeEnv);
	}
	
	//if a directory is given, job target directory and current directory with a backslash and call chdir.
	else{
		char* curDir = malloc(sizeof(char)*2048);
		memset(curDir, '\0', 2048);
		getcwd(curDir, 2048);
		
		strcat(curDir, "/");
		strcat(curDir, targetDir);
		chStatus = chdir(curDir);
		free(curDir);
	}
	if(chStatus != 0){
		perror(targetDir);  //print out any error on screen.
	}
}


/*This is the main program*/
int main(){
	
	struct sigaction ignore_action = {0}, default_action = {0};
	ignore_action.sa_handler = SIG_IGN;
	default_action.sa_handler = SIG_DFL;
	
	//ignore the SIGINT and SIGTERM for parent
	sigaction(SIGINT, &ignore_action, NULL);
	sigaction(SIGTERM, &ignore_action, NULL);
	
	
	//catch SIGCHLD and extract the termination status of the child
	struct sigaction SIGCHLD_action = {0};	
	SIGCHLD_action.sa_handler = catchSIGCHLD;
	sigfillset(&SIGCHLD_action.sa_mask);
	SIGCHLD_action.sa_flags = 0;	
	sigaction(SIGCHLD, &SIGCHLD_action, NULL);
	
	//catch SIGTSTP and extract the termination status of the child
	struct sigaction SIGTSTP_action = {0};	
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;	
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
	
	//this is for storing foreground process status
	char foregroundStatus[200];
	memset(foregroundStatus, '\0', 200);
	strcpy(foregroundStatus, "exit value 0\n");
	
	//get the parent pid
	parentPid = getpid();
	
	//set foreground pid to -5
	foregroundPid = -5;
	
	//Get user input
	int numCharsEntered = -5; // How many chars we entered
	int currChar = -5; // Tracks where we are when we print out every char
	size_t bufferSize = 0; // Holds how large the allocated buffer is
	char* userIn = NULL; // Points to a buffer allocated by getline() that holds our entered string + \n + \0
		
	//the shell loops until exit is received
	int quitNow = 0;
	while(!quitNow){
		
		//if there is a background message, display on screen.
		if(bkMessage != NULL){
			printf("%s", bkMessage);
			free(bkMessage);
			bkMessage = NULL;
			bkCap = 10;
		}
		fflush(stdout);
		
		//prompt for command
		printf(": ");	
		
		while(1){
			//if getline is interrupted by signal, clear stdin and ask for command again.	
			numCharsEntered = getline(&userIn, &bufferSize, stdin);			
			if(numCharsEntered == -1)
				clearerr(stdin);
			else
				break;
		}
		
		//When a command is received, build the command struct and perform corresponding actions.
		if(numCharsEntered != -1){
			
			//Build the command struct
			struct Command* newCommand = BuildCommand(userIn, numCharsEntered);
			if(foregroundOnly == 1)   //edit command to be foreground only
				newCommand->isBkgd = 0;

			//handle built-in functions: exit, cd, status
			if(newCommand != 0){				
				if(userIn[0] == '#'){
					//comment line does nothing
				}else if(strcmp(newCommand->command, "exit") == 0){   //if exit is received, exit the program	
					quitNow = ExitShell();
				}else if(strcmp(newCommand->command, "cd") == 0){    //if cd is received, change current working dir.
					if(newCommand->argumentCt > 1){
						ChangeDir(newCommand->arguments[1]);						
					}else{
						ChangeDir("");
					}
				}else if(strcmp(newCommand->command, "status") == 0){ //if status is received, print the status on screen.
				    printf("%s", foregroundStatus);
				}else{  //for all other commands fork and exec
					pid_t spawnPid = -5;
					spawnPid = fork();
					switch (spawnPid) {
						case -1: { perror("Hull Breach!\n"); exit(1); break; }
						case 0: {
							//all children will ignore SIGTSTP
							sigaction(SIGTSTP, &ignore_action, NULL);
							
							//all children should catch SIGTERM
							sigaction(SIGTERM, &default_action, NULL);
							
							//child should catch SIGINT if it is a foreground
							sigaction(SIGINT, &default_action, NULL);
							
							//child should ignore SIGINT if it is a background
							if(newCommand->isBkgd == 1){
								sigaction(SIGINT, &ignore_action, NULL);
								//redirect stdin/stdout to /dev/null
								int devNull = open("/dev/null", O_RDWR);
								dup2(devNull, 0);
								dup2(devNull, 1);
							}
							
							//redirect to specific input/output
							int sourceFD, targetFD;
							if(newCommand->needInput){								
								sourceFD = open(newCommand->input, O_RDONLY);
								dup2(sourceFD, 0);
							}
							if(newCommand->needOutput){
								targetFD = open(newCommand->output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
								dup2(targetFD, 1);
							}
							
							//exit if error
							if(sourceFD == -1){
								printf("cannot open %s for input\n", newCommand->input);
								exit(1); break;
							}
							if(targetFD == -1){
								printf("cannot open %s for output\n", newCommand->output);
								exit(1); break;
							}
							
							//exec the command
							execvp(newCommand->arguments[0], newCommand->arguments);
							perror(newCommand->arguments[0]);
							exit(1); break;
						}
						default: {
							//block signals until this is done
							sigset_t block_set;
							sigaddset(&block_set, SIGCHLD);
							sigaddset(&block_set, SIGTSTP);
							sigprocmask(SIG_BLOCK, &block_set, NULL);
							
							if(newCommand->isBkgd == 0){
								//foreground process: wait until it is finished and print the exit status
								foregroundPid = spawnPid;
								int childExitStatus = -5;

								pid_t actualPid = waitpid(spawnPid,&childExitStatus,0);
								foregroundPid = -5;
								if(WIFEXITED(childExitStatus)){
									int exitStatus = WEXITSTATUS(childExitStatus);
									memset(foregroundStatus,'\0',200);
									sprintf(foregroundStatus, "exit value %d\n", exitStatus);
								}
								else if(WIFSIGNALED(childExitStatus)){
									int termSignal = WTERMSIG(childExitStatus);
									memset(foregroundStatus,'\0',200);
									sprintf(foregroundStatus, "pid %d terminated(P) by signal %d\n", spawnPid, termSignal);
									printf("%s", foregroundStatus);
								}

							}else{
								//background process: print the pid of the background process.
								printf("background pid is %d\n", spawnPid);
							}
							
							//unblock signals
							sigprocmask(SIG_UNBLOCK, &block_set, NULL);
							
							break;
						}
					}
				}
			}
			
			//free the command structure memory
			FreeCommandStruct(newCommand);
			newCommand = NULL;
		}else{
			clearerr(stdin);
		}
		
		free(userIn);
		userIn = NULL;
		fflush(stdout);

	}
	
	return 0;
}