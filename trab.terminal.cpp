#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

// Headers de interface
#include <ncurses.h>

// Headers de Socket
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;

#include "socketsoper.cpp"

void funcaoBanco (int socket);
int main(void)
{
    string linha;
    char cmd[10];
    char servidor[512];
    int portNo, sockfd;
    hostent *server;
    sockaddr_in serv_addr;
    string msg, cmdE;

    printf("Em qual servidor você deseja conectar o terminal de auto-atendimento?\n");
    getline(cin, linha);

    sscanf(linha.c_str(), "%[A-Za-z0-9:.\\]", servidor);

    printf("Você deseja conectar no servidor %s em que porta?\n", servidor);
    getline(cin, linha);

    sscanf(linha.c_str(), "%d",&portNo);

    printf("Conectando a %s:%d\n", servidor, portNo);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( sockfd < 0 )
    {
	perror("Erro ao abrir socket");
	exit(1);
    }

    server = gethostbyname(servidor);
    if ( server == NULL )
    {
	perror("Erro, host não encontrado");
	exit(2);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    memmove(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    serv_addr.sin_port = htons(portNo);

    if ( connect(sockfd, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0 )
    {
	perror("Erro ao conectar");
	exit(3);
    }

    cmdE = "CONN";
    msg = "SenhaMuitoSecreta!";
    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

    printf("String de conexão enviada...");

    le_socket(sockfd, cmd, sizeof(cmd), msg);

    if ( strcasecmp(cmd, "GOOD") )
    {
	perror("Falha na autenticação com o servidor");
	exit(6);
    }

    printf("Conexão efetuada com o servidor. Iniciando operações\n");

    // -----

    funcaoBanco(sockfd);

    // -----

    cmdE = "COFF";
    msg = "";
    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

    le_socket(sockfd, cmd, sizeof(cmd), msg);

    if ( strcasecmp(cmd, "GOOD") )
    {
    }

    close(sockfd);

    printf("Fechando operações com o servidor.\n");

    return 0;
}

void funcaoBanco (int sockfd)
{
    string linha;
    char conta[15];
    char digital[15];
    char cmd[10];
    string cmdE;
    string msg;

    do
    {

	printf("Bem vindo ao auto-atendimento do Banco Federativo, digite sua conta para se identificar.\n");
	getline(cin, linha);

	sscanf(linha.c_str(), "%5[0-9-]", conta);
	if ( !strcasecmp("Fim", linha.c_str()) )
	{
	    break;
	}

	printf("Agora posicione seu dedo para a identificação da sua digital [Digite sua frase secreta]\n");
	getline(cin, linha);
	sscanf(linha.c_str(), "%10[A-Za-z0-9_]", digital);

	printf("Aguarde enquanto verificamos suas informações...\n");

	cmdE = "LOGA";
	msg = conta + (string)" " + digital;
	esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

	le_socket(sockfd, cmd, sizeof(cmd), msg);

	if ( strcasecmp("GOOD", cmd) )
	{
	    printf("Desculpe, houve um erro: %s\n", msg.c_str());
	    continue;
	}

	printf("Bem vindo, %s.\n", msg.c_str());
	do
	{
	    int opcode, n;
	    printf("O que você deseja fazer hoje?\n");
	    printf("Digite 1 para saque.\n");
	    printf("Digite 2 para saldo.\n");
	    printf("Digite 3 para transferências.\n");
	    printf("Digite 4 caso não queira mais fazer operações.\n");
	    getline(cin, linha);
	    n = sscanf(linha.c_str(), "%d", &opcode);

	    if ( n != 1 )
	    {
		printf("Opção desconhecida!\n");
		continue;
	    }

	    bool quit = false;

	    switch (opcode)
	    {
		case 1:
		    {
			int opcao;
			printf("Quanto você gostaria de sacar?\n");
			printf("1 - R$10\n");
			printf("2 - R$20\n");
			printf("3 - R$50\n");
			printf("4 - R$100\n");
			printf("5 - R$500\n");
			getline(cin, linha);
			n = sscanf(linha.c_str(), "%d", &opcao);
			if ( n != 1 )
			{
			    printf("Opção desconhecida.\n");
			    break;
			}
			switch(opcao)
			{
			    case 1:
				msg = "10";
				break;
			    case 2:
				msg = "20";
				break;
			    case 3:
				msg = "50";
				break;
			    case 4:
				msg = "100";
				break;
			    case 5:
				msg = "500";
				break;
			    default:
				msg = "";
				printf("Opção desconhecida.\n");
				break;
			}
			if ( msg == "" )
			{
			    break;
			}

			cmdE = "SAQU";
			esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

			le_socket(sockfd, cmd, sizeof(cmd), msg);

			if ( strcasecmp(cmd, "GOOD") )
			{
			    printf("Houve um erro: %s", msg.c_str());
			    break;
			}

			printf("Retire o seu dinheiro!\n");
		    }
		    break;
		case 2:
		    cmdE = "SALD";
		    msg = "";
		    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

		    msg = "";
		    cmd[0] = '\0';
		    le_socket(sockfd, cmd, sizeof(cmd), msg);

		    if ( strcasecmp(cmd, "GOOD") )
		    {
			printf("Houve um erro: %s\n", msg.c_str());
			break;
		    }

		    printf("Você tem R$%s\n", msg.c_str());
		    break;
		case 3:
		    {
			int decisao;
			printf("Para transferir para contas do mesmo banco, pressione 1\n");
			printf("Para contas de outros bancos(DOC), pressione 2\n");
			getline(cin, linha);
			sscanf(linha.c_str(), "%d", &decisao);
			if (decisao == 1)
			{
			    int valor;
			    char conta[20];

			    printf("Pra qual conta você gostaria de transferir?\n");
			    getline(cin, linha);
			    sscanf(linha.c_str(), "%5[0-9-]", conta);

			    cmdE = "ITR1";
			    msg = conta;

			    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

			    msg = "";
			    cmd[0] = '\0';
			    le_socket(sockfd, cmd, sizeof(cmd), msg);

			    if ( strcasecmp(cmd, "GOOD") )
			    {
				printf("Erro: %s", msg.c_str());
				break;
			    }

			    printf("Quanto você gostaria de transferir?\n");
			    getline(cin, linha);
			    n = sscanf(linha.c_str(), "%d", &valor);
			    if ( n != 1 )
			    {
				printf("É necessário digitar um número.\n");
				break;
			    } // n != 1

			    char buffer[10];

			    sprintf(buffer, "%d", valor);

			    msg = buffer;

			    cmdE = "ITR2";
			    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

			    msg = "";
			    cmd[0] = '\0';
			    le_socket(sockfd, cmd, sizeof(cmd), msg);

			    if ( strcasecmp(cmd, "GOOD") )
			    {
				printf("Erro: %s", msg.c_str());
				break;
			    }

			    printf("O dinheiro foi transferido!\n");
			} // decisao == 1 
			else if ( decisao == 2 )
			{
			    int valor, opcao, n;
			    char conta[20];

			    printf("Pra qual banco você gostaria de transferir?\n");
			    printf("1 - Banco 2\n");
			    getline(cin, linha);
			    n = sscanf(linha.c_str(), "%d", opcao);

			    if ( n != 1 )
			    {
				printf("É preciso escolher um banco\n");
				break;
			    }

			    if ( opcao != 1 )
			    {
				printf("Opção desconhecida\n");
				break;
			    }

			    printf("Pra qual conta você gostaria de transferir?\n");
			    getline(cin, linha);
			    sscanf(linha.c_str(), "%5[0-9-]", conta);

			    cmdE = "ETR1";
			    msg = conta + " 1";

			    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

			    msg = "";
			    cmd[0] = '\0';
			    le_socket(sockfd, cmd, sizeof(cmd), msg);

			    if ( strcasecmp(cmd, "GOOD") )
			    {
				printf("Erro: %s", msg.c_str());
				break;
			    }

			    printf("Quanto você gostaria de transferir?\n");
			    getline(cin, linha);
			    n = sscanf(linha.c_str(), "%d", &valor);
			    if ( n != 1 )
			    {
				printf("É necessário digitar um número.\n");
				break;
			    } // n != 1

			    char buffer[10];

			    sprintf(buffer, "%d", valor);

			    msg = buffer;

			    cmdE = "ETR2";
			    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

			    msg = "";
			    cmd[0] = '\0';
			    le_socket(sockfd, cmd, sizeof(cmd), msg);

			    if ( strcasecmp(cmd, "GOOD") )
			    {
				printf("Erro: %s", msg.c_str());
				break;
			    }

			    printf("O dinheiro foi transferido!\n");
			} // decisao == 2
			else
			{
			    printf("Opcao desconhecida");
			} // else
		    } // case 3 
		    break;
		case 4:
		    cmdE = "LOFF";
		    msg = "";
		    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
		    quit = true;
		    break;
		default:
		    printf("Opção desconhecida.\n");
		    break;
	    } // switch (opcode)

	    if ( quit )
		break;
	} while (1);
    } while (1);
}
