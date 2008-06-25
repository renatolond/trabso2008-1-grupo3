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

#define TAMSENHA 10
#define TAMNOME 20
#define TAMINTS 4

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
    int valorTransf;
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

void escAccInfo(accInfo& acc)
{
    char buffer[TAMNOME+2];

    fstream contaArq;
    contaArq.open((acc.conta + ".acc").c_str(), fstream::out | fstream::binary);

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, acc.senha.c_str(), TAMSENHA);

    contaArq.write(buffer, TAMSENHA);

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, acc.nome.c_str(), TAMNOME);
    contaArq.write(buffer, TAMNOME);

    contaArq.write((char *)&acc.saldo, TAMINTS);

    contaArq.write((char *)&acc.centavos, TAMINTS);

    contaArq.write((char *)&acc.saques, TAMINTS);

    contaArq.write((char *)&acc.valor, TAMINTS);

    contaArq.write((char *)&acc.titulares, TAMINTS);

    contaArq.write((char *)&acc.valorTransf, TAMINTS);

    int tam;

    tam = acc.log.size();

    contaArq.write((char *)&tam, TAMINTS);

    contaArq.write(acc.log.c_str(), tam);
    tam = 0;
    contaArq.write((char*)&tam, 1);

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
    contaArq.read(buffer, TAMSENHA);
    acc.senha = buffer;

    // lê nome do titular da conta
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, TAMNOME);
    acc.nome = buffer;

    // lê o saldo da conta
    acc.saldo = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, TAMINTS);
    memcpy(&acc.saldo, buffer, TAMINTS);

    // lê os centavos da conta
    acc.centavos = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, TAMINTS);
    memcpy(&acc.centavos, buffer, TAMINTS);

    // lê quantos saques foram feitos no dia
    acc.saques = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, TAMINTS);
    memcpy(&acc.saques, buffer, TAMINTS);

    // lê qual o valor acumulado dos saques feitos no dia
    acc.valor = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, TAMINTS);
    memcpy(&acc.valor, buffer, TAMINTS);

    // lê o número de titulares da conta
    acc.titulares = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, TAMINTS);
    memcpy(&acc.titulares, buffer, TAMINTS);

    // lê o valor das transferências da conta
    acc.valorTransf = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, TAMINTS);
    memcpy(&acc.valorTransf, buffer, TAMINTS);

    tam = 0;
    memset(buffer, 0, sizeof(buffer));
    contaArq.read(buffer, 4);
    memcpy(&tam, buffer, 4);

    if ( tam > 0 )
    {
	char *log = new char[tam+2];

	memset(log, 0, tam+2);
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
    int newsockfd;
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

    accInfo cliente, cliente2;

    cliente.initialized = false;

    do
    {
	// Fica esperando o recebimento de dados até que receba o comando para sair.
	le_socket(sockfd, cmd, sizeof(cmd), msg);

	if ( !strcasecmp(cmd, "COFF") )
	{
	    break;
	}
	else if ( !strcasecmp(cmd, "LOFF") )
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
	else if ( !strcasecmp(cmd, "SAQU") )
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
		cliente.saldo -= quantia;
		cliente.log += "Saque no valor de R$" + msg +"\n";
		cliente.saques++;
		cliente.valor+=quantia;
		escAccInfo(cliente);
		cmdE = "GOOD";
		msg = "";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
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
	else if ( !strcasecmp(cmd, "ETR1") )
	{
	    char conta[10];
	    int banco;

	    sscanf("%s %d", conta, &banco);

	    // Isso é escalonável para quantos bancos existirem, mas para isso, os bancos deveriam ter
	    // vários ips e/ou portas diferentes. No caso do nosso teste, o "Banco 2" sempre será o 
	    // banco cuja porta é 28333
#define SERVIDORBANCO2 "localhost"
#define PORTABANCO2 28333

	    newsockfd = socket(AF_INET, SOCK_STREAM, 0);
	    if ( newsockfd < 0 )
	    {
		perror("Erro ao abrir socket");
		exit(1);
	    }

	    server = gethostbyname(SERVIDORBANCO2);
	    if ( server == NULL )
	    {
		perror("Erro, host não encontrado");
		exit(2);
	    }

	    memset(&serv_addr, 0, sizeof(serv_addr));
	    serv_addr.sin_family = AF_INET;

	    memmove(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	    serv_addr.sin_port = htons(PORTABANCO2);

	    if ( connect(newsockfd, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0 )
	    {
		perror("Erro ao conectar");
		exit(3);
	    }

	    cmdE = "CONN";
	    msg = "SenhaMuitoSecreta!";
	    esc_socket(newsockfd, cmdE.c_str(), cmdE.size(), msg);

	    le_socket(newsockfd, cmd, sizeof(cmd), msg);

	    if ( strcasecmp(cmd, "GOOD") )
	    {
		printf("Falha na autenticação com o servidor\n");
		cmdE = "NONO";
		msg = "Erro de comunicação com o Banco 2\n";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
		continue;
	    }

	    cmdE = "CHCO"; // checa conta
	    msg = conta;
	    esc_socket(newsockfd, cmdE.c_str(), cmdE.size(), msg);

	    msg = "";
	    cmd[0] = '\0';

	    le_socket(newsockfd, cmd, sizeof(cmd), msg);

	    if ( strcasecmp(cmd, "GOOD") )
	    {
		cmdE = "NONO";
		msg = "Não existe tal conta no Banco 2\n";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
		continue;
	    }

	    cmdE = "GOOD";
	    msg = "";
	    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	}
	else if ( !strcasecmp(cmd, "ETR2") )
	{
	    int quantia;

	    sscanf(msg.c_str(), "%d", &quantia);
	    cmdE = "RDOC";
	    esc_socket(newsockfd, cmdE.c_str(), cmdE.size(), msg);

	    le_socket(newsockfd, cmd, sizeof(cmd), msg);

	    cmdE = "COFF";

	    esc_socket(newsockfd, cmdE.c_str(), cmdE.size(), msg);

	    if ( strcasecmp(cmd, "GOOD") )
	    {
		cmdE = "NONO";
		msg = "Houve um problema no DOC para o banco 2.\n";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
		continue;
	    }

	    cmdE = "GOOD";
	    msg = "";
	    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

	    close(newsockfd);
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


		cliente.saldo -= quantia - 10;
		cliente.log += "Doc enviado no valor de R$" + msg +"\n";
		cliente.valorDoc += quantia;
		escAccInfo(cliente);
		cmdE = "GOOD";
		msg = "";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

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
	else if ( !strcasecmp(cmd, "RDOC") )
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

		cliente.saldo += quantia;
		cliente.log += "Doc recebido no valor de R$" + msg +"\n";
		escAccInfo(cliente);
		cmdE = "GOOD";
		msg = "";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);

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
	else if ( !strcasecmp(cmd, "CHCO") )
	{
	    string conta = msg;
	    if ( !fileExists(conta + (string)".acc") )
	    {
		cmdE = "NONO";
		msg = "Conta não existe\n";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }

	    cliente.conta = conta;

	    cmdE = "GOOD";
	    msg = "";
	    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	}
	else if ( !strcasecmp(cmd, "ITR1") )
	{
	    string conta = msg;

	    if ( conta == cliente.conta )
	    {
		cmdE = "NONO";
		msg = "Você não pode fazer uma transferência para si mesmo\n";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }

	    if ( !fileExists(conta + (string)".acc") )
	    {
		cmdE = "NONO";
		msg = "Conta não existe\n";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }

	    cliente2.conta = msg;

	    cmdE = "GOOD";
	    msg = "";
	    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	}
	else if ( !strcasecmp(cmd, "ITR2") )
	{
	    do
	    {
		bool emUso = false;
		pthread_mutex_lock(&mutexContas);
		for ( int i = 0 ; i < contasEmUso.size() ; i++ )
		{
		    if ( contasEmUso.at(i) == cliente.conta || contasEmUso.at(i) == cliente2.conta )
		    {
			emUso = true;
			break;
		    }
		}

		if ( !emUso )
		    break;
		pthread_mutex_unlock(&mutexContas);
	    } while ( 1 );

	    // Bloqueia as contas enquanto escreve...
	    contasEmUso.push_back(cliente.conta);
	    contasEmUso.push_back(cliente2.conta);

	    leAccInfo(cliente.conta, cliente);
	    leAccInfo(cliente2.conta, cliente2);

	    pthread_mutex_unlock(&mutexContas);

	    int quantia;

	    sscanf(msg.c_str(), "%d", &quantia);

	    if ( cliente.saldo < quantia )
	    {
		cmdE = "NONO";
		msg = "Não há saldo suficiente para esta operação!";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }
	    else if ( cliente.valorTransf+quantia >= 5000 )
	    {
		cmdE = "NONO";
		msg = "O valor das transferências desta conta já excedeu R$5000";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }
	    else
	    {
		cliente.saldo -= quantia;
		cliente.log += "Transferencia Efetuada no valor de R$" + msg +"\n";
		cliente.valorTransf+=quantia;
		cliente2.saldo += quantia;
		cliente2.log += "Transferencia Recebida no valor de R$" + msg + "\n";
		escAccInfo(cliente);
		escAccInfo(cliente2);
		cmdE = "GOOD";
		msg = "";
		esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
	    }

	    // Depois da escrita libera as contas
	    pthread_mutex_lock(&mutexContas);
	    for ( int i = 0 ; i < contasEmUso.size() ; i++ )
	    {
		if ( contasEmUso.at(i) == cliente.conta || contasEmUso.at(i) == cliente2.conta )
		{
		    contasEmUso.erase(contasEmUso.begin() + i);
		    i--;
		}
	    }
	    pthread_mutex_unlock(&mutexContas);

	}
	else if ( !strcasecmp(cmd, "SALD") )
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
	else if ( !strcasecmp(cmd, "LOGA") )
	{
	    char conta[15];
	    char digital[15];
	    bool erro;
	    sscanf(msg.c_str(), "%s %s", conta, digital);
	    erro = false;
	    pthread_mutex_lock(&mutexLogados);
	    for ( int i = 0 ; i < clientesLogados.size() ; i++ )
	    {
		if ( clientesLogados.at(i) == conta && cliente.titulares <= 1 )
		{
		    cmdE = "NONO";
		    msg = "Atenção! Esta conta está aberta em outro terminal de atendimento!";
		    esc_socket(sockfd, cmdE.c_str(), cmdE.size(), msg);
		    erro = true;
		    break;
		}
	    }
	    pthread_mutex_unlock(&mutexLogados);
	    if ( erro == true )
		continue;

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

