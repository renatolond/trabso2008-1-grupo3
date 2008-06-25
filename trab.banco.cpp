#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// Headers de Socket
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

// Headers de thread
#include <pthread.h>

using namespace std;
#include "socketsoper.cpp"

// responsável pela região de exclusão mútua de quais arquivos estão em uso
pthread_mutex_t mutexContas = PTHREAD_MUTEX_INITIALIZER;
// responsável pela região de exclusão mútua de quais clientes estão logados
pthread_mutex_t mutexLogados = PTHREAD_MUTEX_INITIALIZER;
vector<string> contasEmUso, clientesLogados;

struct _accInfo
{
    string conta;
    string senha;
    string nome;
    int saldo, centavos;
    int saques, valor;
    int titulares;
    string log;
    bool initialized;
};

typedef _accInfo accInfo;

void * funcaoBanco (void * msg);

template<typename T> std::string toString(const T& t) {
    std::stringstream s;
    s << t;
    return s.str();
}

void leAccInfo(string conta, accInfo& acc)
{
    char buffer[50];
    int tam;
    fstream contaArq;
    contaArq.open((conta + ".acc").c_str(), fstream::in | fstream::binary);
    acc.conta= conta;

    contaArq.seekg(0, ios::beg);

    // lê senha da conta
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 10);
    acc.senha = buffer;

    // lê nome do titular da conta
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 20);
    acc.nome = buffer;

    // lê o saldo da conta
    acc.saldo = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 4);
    memcpy(&acc.saldo, buffer, 4);

    // lê os centavos da conta
    acc.centavos = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 4);
    memcpy(&acc.centavos, buffer, 4);
    
    // lê quantos saques foram feitos no dia
    acc.saques = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 4);
    memcpy(&acc.saques, buffer, 4);
    
    // lê qual o valor acumulado dos saques feitos no dia
    acc.valor = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 4);
    memcpy(&acc.valor, buffer, 4);

    tam = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 4);
    memcpy(&tam, buffer, 4);

    if ( tam > 0 )
    {
	char *log = new char[tam+2];

	memset(log, 0, sizeof(buffer));
	contaArq.read(log, tam);
	acc.log = log;

	delete[] log;
    }
    else
	acc.log = "";

    contaArq.close();
}

bool fileExists(const std::string& fileName)
{
    std::fstream fin;
    fin.open(fileName.c_str(),std::ios::in);
    if( fin.is_open() )
    {
	fin.close();
	return true;
    }
    fin.close();
    return false;
}

int main(void)
{
    int sockfd, newsockfd, portNo;
    sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    pthread_t thread;
    vector<pthread_t> threads;

    contasEmUso.clear();
    clientesLogados.clear();

    printf("Em qual porta você deseja que o servidor seja iniciado?\n");
    scanf("%d", &portNo);
    printf("Iniciando servidor...\n");

    // Cria um socket em modo stream para a internet
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
	perror("Erro ao abrir socket");
	exit(1);
    }

    // Seta os dois endereços como zero
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    // Preenche o endereço do servidor com as informações necessárias
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portNo);

    // Tenta associar o socket com um endereço
    if (bind(sockfd, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
	perror("Erro no bind, será que a porta está ocupada?");
	exit(2);
    }

    // Coloca o socket em modo listening
    listen(sockfd,5);
    do
    {
	clilen = sizeof(cli_addr);

	printf("Aguardando conexão...\n");

	// Accept deixa o programa esperando até que alguém tente conectar na porta associada ao socket
	newsockfd = accept(sockfd, (sockaddr *) &cli_addr, &clilen);

	// Se houver um problema ao tentar criar o novo socket
	if (newsockfd < 0)
	{
	    perror("Erro ao aceitar conexão");
	    exit(3);
	}

	printf("Conexão efetuada, disparando thread...\n");

	// Cria a thread para tratar do novo terminal conectado
	pthread_create(&thread, NULL, funcaoBanco, (void*) &newsockfd);
	threads.push_back(thread);
	//	pthread_join(thread1, NULL);

	//	close(newsockfd);
	//	memset(&cli_addr, 0, sizeof(cli_addr));
    }
    while(1);

    close(sockfd);

    return 0;
}

void * funcaoBanco (void * param)
{
    int sockfd = *((int*) param);
    char cmd[10];
    string cmdE;
    string msg;

    printf("Aguardando string de conexão...\n");

    le_socket(sockfd, cmd, sizeof(cmd), msg);

    // Vê se o primeiro comando recebido é o comando de conexão
    if ( strcasecmp(cmd, "CONN") )
    {
	printf("Tentativa não autorizada de acesso. Fechando conexão.\n");

	return NULL;
    }

    // Confere a senha
    if ( msg != "SenhaMuitoSecreta!" )
    {
	perror("Tentativa não autorizada de acesso. Fechando conexão.\n");

	return NULL;
    }

    // Envia o comando de OK
    strcpy(cmd, "GOOD");
    msg = "";
    esc_socket(sockfd, cmd, sizeof(cmd), msg);

    printf("OK, o caixa de auto-atendimento se identificou com sucesso. Iniciando operações.\n");

    accInfo cliente;

    cliente.initialized = false;

    do
    {
	// Fica esperando o recebimento de dados até que receba o comando para sair.
	le_socket(sockfd, cmd, sizeof(cmd), msg);

	if ( !strcasecmp(cmd, "COFF") )
	{
	    break;
	}
	if ( !strcasecmp(cmd, "LOFF") )
	{
	    cliente.initialized = false;
	    pthread_mutex_lock(&mutexLogados);
	    for ( int i = 0 ; i < clientesLogados.size() ; i++ )
	    {
		if ( clientesLogados.at(i) == cliente.conta )
		{
		    clientesLogados.erase(clientesLogados.begin() + i);
		    break;
		}
	    }
	    pthread_mutex_unlock(&mutexLogados);
	    cmdE = "GOOD";
	    msg = "Tenha um bom dia!";
	    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	}
	if ( !strcasecmp(cmd, "SAQU") )
	{
	    do
	    {
		bool emUso = false;
		pthread_mutex_lock(&mutexContas);
		for ( int i = 0 ; i < contasEmUso.size() ; i++ )
		{
		    if ( contasEmUso.at(i) == cliente.conta )
		    {
			emUso = true;
			break;
		    }
		}

		if ( !emUso )
		    break;
		pthread_mutex_unlock(&mutexContas);
	    } while ( 1 );

	    // Bloqueia a conta enquanto escreve...
	    contasEmUso.push_back(cliente.conta);
	    leAccInfo(cliente.conta, cliente);

	    pthread_mutex_unlock(&mutexContas);

	    int quantia;

	    sscanf(msg.c_str(), "%d", &quantia);

	    if ( cliente.saldo < quantia )
	    {
		    cmdE = "NONO";
		    msg = "Não há saldo suficiente para esta operação!";
		    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }
	    else if ( cliente.saques >= 3 )
	    {
		cmdE = "NONO";
		msg = "Já foram efetuados 3 saques nesta conta.";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }
	    else if ( cliente.valor+quantia >= 1000 )
	    {
		cmdE = "NONO";
		msg = "O valor dos saques desta conta já excedeu R$1000";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }
	    else
	    {
	    }

	    // Depois da escrita libera a conta
	    pthread_mutex_lock(&mutexContas);
	    for ( int i = 0 ; i < contasEmUso.size() ; i++ )
	    {
		if ( contasEmUso.at(i) == cliente.conta )
		{
		    contasEmUso.erase(contasEmUso.begin() + i);
		    break;
		}
	    }
	    pthread_mutex_unlock(&mutexContas);
	}

	if ( !strcasecmp(cmd, "SALD") )
	{
	    do
	    {
		bool emUso = false;
		pthread_mutex_lock(&mutexContas);
		for ( int i = 0 ; i < contasEmUso.size() ; i++ )
		{
		    if ( contasEmUso.at(i) == cliente.conta )
		    {
			emUso = true;
			break;
		    }
		}

		if ( !emUso )
		    break;
		pthread_mutex_unlock(&mutexContas);
	    } while ( 1 );

	    leAccInfo(cliente.conta, cliente);

	    pthread_mutex_unlock(&mutexContas);

	    cmdE = "GOOD";
	    msg = toString(cliente.saldo) + "." + toString(cliente.centavos);
	    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	}
	if ( !strcasecmp(cmd, "LOGA") )
	{
	    char conta[15];
	    char digital[15];
	    sscanf(msg.c_str(), "%s %s", conta, digital);
	    pthread_mutex_lock(&mutexLogados);
	    for ( int i = 0 ; i < clientesLogados.size() ; i++ )
	    {
		if ( clientesLogados.at(i) == conta )
		{
		    cmdE = "NONO";
		    msg = "Atenção! Esta conta está aberta em outro terminal de atendimento!";
		    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
		    break;
		}
	    }
	    pthread_mutex_unlock(&mutexLogados);

	    if ( !fileExists(conta + (string)".acc") )
	    {
		cmdE = "NONO";
		msg = "Conta não existe";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }

	    do
	    {
		bool emUso = false;
		pthread_mutex_lock(&mutexContas);
		for ( int i = 0 ; i < contasEmUso.size() ; i++ )
		{
		    if ( contasEmUso.at(i) == conta )
		    {
			emUso = true;
			break;
		    }
		}

		if ( !emUso )
		    break;
		pthread_mutex_unlock(&mutexContas);
	    } while ( 1 );

	    leAccInfo(conta, cliente);

	    pthread_mutex_unlock(&mutexContas);

	    if ( strcmp(cliente.senha.c_str(), digital) )
	    {
		cmdE = "NONO";
		msg = "Digital não confere";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }

	    cmdE = "GOOD";
	    msg = cliente.nome;
	    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    pthread_mutex_lock(&mutexLogados);
	    clientesLogados.push_back(conta);
	    pthread_mutex_unlock(&mutexLogados);
	}
    } while ( 1 );

    printf("Fechando operações com o caixa de auto-atendimento\n");
    close(sockfd);
}

