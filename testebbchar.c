/**
 * @file   testebbchar.c
 * @author Derek Molloy
 * @date   7 April 2015
 * @version 0.1
 * @brief  A Linux user space program that communicates with the ebbchar.c LKM. It passes a
 * string to the LKM and reads the response from the LKM. For this example to work the device
 * must be called /dev/ebbchar.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
*/
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<string.h>

#define BUFFER_LENGTH 16               ///< The buffer length (crude but fine)
static char receive[2*BUFFER_LENGTH+3];     ///< The receive buffer from the LKM


int toString(int n)
{
if(n>9)
{
n+=87;

}
else
{
n+=48;
}
return n;
}



int main(){
   int ret, fd;
	int tipo=0,opcao=0,tam=0;
   char stringToSend[2*BUFFER_LENGTH+3];
   char lerHex[2*BUFFER_LENGTH+1];
   char converter[BUFFER_LENGTH];
	int j=2;
	
   

	
   printf("Starting device test code example...\n");
   fd = open("/dev/ebbchar", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }

	printf("1- Criptografar \n 2- Descriptografar \n 3- Hash \n 4- Cancelar \nSelecione uma opção:");
	scanf("%d",&tipo);
	printf("1- Entrada em string \n 2- Entrada em hexa \n selecione uma opção:");
	scanf("%d",&opcao);
	
	switch(tipo)
	{
	 case 1:
	stringToSend[0]='c';
	stringToSend[1]=' ';	
	break;
	case 2:
	stringToSend[0]='d';
	stringToSend[1]=' ';
	break;
	

	}
	
	if(opcao==1)
	{

	printf("digite uma string para o kernel \n");
	__fpurge(stdin);
	scanf("%[^\n]%*c", converter);
	tam=strlen(converter);
	
	for(int k=tam;k<BUFFER_LENGTH;k++)
	{
	 converter[k]='0';
	}
	

	for(int i=0;i<BUFFER_LENGTH;i++)
	{
	stringToSend[j]=toString((int )converter[i]/16);
	j++;
	stringToSend[j]=toString((int )converter[i]%16);
	j++;
	}
	stringToSend[j]='\0';
	

	}
else
{
printf("digite uma string para o kernel \n");
__fpurge(stdin);
scanf("%[^\n]%*c", lerHex);
tam=strlen(lerHex);
printf("\n tam %d",tam);
for(int k=tam;k<2*BUFFER_LENGTH;k++)
	{
	printf("\n %d",k);
	 lerHex[k]='3';
	k++;
	lerHex[k]='0';
	}


for(int i=0;i<2*BUFFER_LENGTH;i++)
{
 stringToSend[j]=lerHex[i];
j++;
}
stringToSend[j]='\0';


}
	printf("%d fgdgf \n",strlen(stringToSend));
   printf("Writing message to the device [%s].\n", stringToSend);
   ret = write(fd, stringToSend, strlen(stringToSend)); // Send the string to the LKM
   if (ret < 0){
      perror("Failed to write the message to the device.");
      return errno;
   }

   printf("Press ENTER to read back from the device...\n");
   getchar();

   printf("Reading from the device...\n");
   ret = read(fd, receive, BUFFER_LENGTH);        // Read the response from the LKM
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return errno;
   }
   printf("The received message is: [%s]\n", receive);
   printf("End of the program\n");
   return 0;
}
