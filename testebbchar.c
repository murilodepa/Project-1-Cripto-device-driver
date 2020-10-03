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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_LENGTH 16 ///< The buffer length (crude but fine)
static char receive[40]; ///< The receive buffer from the LKM

#define clear() printf("\033[H\033[J")

/* Converter String */
int toString(int n)
{
	if (n > 9)
	{
		n += 87;
	}
	else
	{
		n += 48;
	}
	return n;
}

/* Função Principal */
int main()
{
	int ret, fd;
	int tipo = 0, opcao = 0, tam = 0;
	char stringToSend[2 * BUFFER_LENGTH + 3];
	char lerHex[2 * BUFFER_LENGTH + 1];
	char converter[BUFFER_LENGTH];
	int j = 2;

	printf("Starting device test code example...\n");
	fd = open("/dev/ebbchar", O_RDWR); // Open the device with read/write access
	
	if (fd < 0)
	{
		perror("Failed to open the device...");
		return errno;
	}

	do
	{
		//zerando variavel
		j = 2;

		printf("Selecione o que deseja fazer.\n");
		printf(" 1- Criptografar \n 2- Descriptografar \n 3- Hash \n 4- Cancelar \nSelecione uma opção:");
		scanf("%d", &tipo);

		if (tipo != 4) // Se for diferente do tipo 4 (função "cancelar" para finalizar o programa)
		{
			if (tipo != 3) // Se usuário escolher criptografia ou descriptografia
			{
				printf("\nSelecione o tipo de entrada desejada.\n");
				printf(" 1- Entrada em string \n 2- Entrada em hexa \n Selecione uma opção:");
				scanf("%d", &opcao);
			}
			else
			{
				opcao = 1;
			}

			switch (tipo)
			{
			case 1:
				stringToSend[0] = 'c'; // Criptografia
				stringToSend[1] = ' ';
				break;
			case 2:
				stringToSend[0] = 'd'; // Descriptografia
				stringToSend[1] = ' ';
				break;
			case 3:
				stringToSend[0] = 'h'; // Hash
				stringToSend[1] = ' ';
				break;
			}

			if (opcao == 1)
			{
				printf("Digite uma string para o kernel: \n");
				__fpurge(stdin);
				scanf("%[^\n]%*c", converter);
				tam = strlen(converter);

				if (tipo != 3) // Se tipo for diferente da opção de Hash
				{ // 'c' ou 'd'
					for (int k = tam; k < BUFFER_LENGTH; k++)
					{
						converter[k] = '0';
					}
					
					/* Converte String para Hexadecimal */
					for (int i = 0; i < BUFFER_LENGTH; i++)
					{
						stringToSend[j] = toString((int)converter[i] / 16);
						j++;
						stringToSend[j] = toString((int)converter[i] % 16);
						j++;
					}
					stringToSend[j] = '\0';
				}
				else
				{ // Realiza o cálculo do valor do Hash, "SHA1 (CryptoLogic SHA1 valor; 40 caracteres)"
					/* Converte String para Hexadecimal */
					for (int i = 0; i < tam; i++)
					{
						stringToSend[j] = toString((int)converter[i] / 16);
						j++;
						stringToSend[j] = toString((int)converter[i] % 16);
						j++;
					}
					stringToSend[j] = '\0';
				}
			}
			else
			{
				printf("Digite uma string para o kernel:\n");
				__fpurge(stdin);
				scanf("%[^\n]%*c", lerHex);
				tam = strlen(lerHex);

				for (int k = tam; k < 2 * BUFFER_LENGTH; k++)
				{
					lerHex[k] = '3';
					k++;
					lerHex[k] = '0';
				}

				for (int i = 0; i < 2 * BUFFER_LENGTH; i++)
				{
					stringToSend[j] = lerHex[i];
					j++;
				}
				stringToSend[j] = '\0';
			}
			
			printf("\nWriting message to the device [%s].\n", stringToSend);
			ret = write(fd, stringToSend, strlen(stringToSend)); // Send the string to the LKM
			
			if (ret < 0)
			{
				perror("Failed to write the message to the device.");
				return errno;
			}

			printf("Press ENTER to read back from the device...\n");
			getchar();
			printf("Reading from the device...\n");
			ret = read(fd, receive, BUFFER_LENGTH); // Read the response from the LKM
			
			if (ret < 0)
			{
				perror("Failed to read the message from the device.");
				return errno;
			}
			
			printf("The received message is: [");
			int impressao = 0;
			
			if (stringToSend[0] == 'c' || stringToSend[0] == 'd')
			{
				impressao = 32; // Trabalha com 32 em criptografia e descriptografia
			}
			else
			{
				impressao = 40; // Caso Hash, o CryptoLogic SHA1 valor, trabalha com 40 caracteres
			}
			
			for (int i = 0; i < impressao; i++)
			{
				printf("%c", receive[i]);
			}
			printf("] \n");
		}
		
		printf("\nPress ENTER to clean the screen...\n");
		getchar();
		clear();
		
	} while (tipo != 4); // Verifica até o usuário escolher a opção cancelar para finalizar o programa
	
	printf("End of the program\n");
	return 0;
}