// Baseado no codigo em https://medium.com/@tristan219/create-a-web-client-with-c-da7ff9210c31

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> // funcoes para sockets
#include <netdb.h> // para resolver nomes de host (DNS)
#include <sys/stat.h> // operacoes com arquivos
#include <errno.h> // mensagens de erro
#include <ctype.h> // isdigit
#include <inttypes.h> // int64_t, PRI

#define BUFFER_TAMANHO 4096 // tamanho do buffer para leitura de dados
#define CABECALHO_TAMANHO 8192 // tamanho maximo do cabecalho HTTP

void extraiHostPortaECaminhoDaURL(const char *urlCompleta, char *hostDoServidor, int *portaDoServidor, char *caminhoDoArquivo) {
    *portaDoServidor = 80; // porta padrao HTTP
    strcpy(caminhoDoArquivo, "/"); // caminho padrao

    if(strncmp(urlCompleta, "http://", 7) == 0)
        urlCompleta += 7; // remove prefixo http://

    const char *inicioDoCaminho = strchr(urlCompleta, '/'); // procura inicio do caminho
    const char *inicioDaPorta = strchr(urlCompleta, ':'); // procura porta

    if(inicioDaPorta && (!inicioDoCaminho || inicioDaPorta < inicioDoCaminho)) { // porta antes do caminho
        strncpy(hostDoServidor, urlCompleta, inicioDaPorta - urlCompleta);
        hostDoServidor[inicioDaPorta - urlCompleta] = '\0';
        *portaDoServidor = atoi(inicioDaPorta + 1);
    } else if(inicioDoCaminho) { // host + caminho, sem porta
        strncpy(hostDoServidor, urlCompleta, inicioDoCaminho - urlCompleta);
        hostDoServidor[inicioDoCaminho - urlCompleta] = '\0';
    } else { // apenas host
        strcpy(hostDoServidor, urlCompleta);
    }

    if(inicioDoCaminho)
        strcpy(caminhoDoArquivo, inicioDoCaminho); // copia caminho completo
}

void exibeProgressoDoDownload(int64_t bytesBaixados, int64_t tamanhoTotalDoArquivo) { // progresso do download
    if(tamanhoTotalDoArquivo <= 0) 
        return;
    int porcentagemBaixada = (int)((bytesBaixados * 100) / tamanhoTotalDoArquivo); // calcula porcentagem
    printf("\rProgresso: %d%%", porcentagemBaixada);
    fflush(stdout);
}

char *procuraSubstring(const char *haystack, const char *needle) { // busca substring ignorando maiusculas/minusculas
    size_t needle_len = strlen(needle); // tamanho da substring
    for(; *haystack; haystack++) { // percorre a string principal
        if(strncasecmp(haystack, needle, needle_len) == 0) // compara ignorando case
            return (char*)haystack; // retorna ponteiro para a posicao encontrada
    }
    return NULL;
}

void realizaDownloadHTTP(const char *hostDoServidor, int portaDoServidor, const char *caminhoDoArquivoNoServidor, const char *nomeArquivoSaidaLocal) {

    struct hostent *informacoesHost = gethostbyname(hostDoServidor); // resolve host
    if(!informacoesHost) {
        fprintf(stderr, "Erro: host nao encontrado: %s\n", hostDoServidor);
        exit(EXIT_FAILURE);
    }

    int socketClienteHTTP = socket(AF_INET, SOCK_STREAM, 0); // cria socket TCP
    if(socketClienteHTTP < 0) {
        perror("Erro ao criar socket TCP");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in enderecoServidorHTTP;
    enderecoServidorHTTP.sin_family = AF_INET; // ipv4
    enderecoServidorHTTP.sin_port = htons(portaDoServidor); // porta
    memcpy(&enderecoServidorHTTP.sin_addr.s_addr, informacoesHost->h_addr_list[0], informacoesHost->h_length); // copia IP resolvido

    if(connect(socketClienteHTTP, (struct sockaddr *)&enderecoServidorHTTP, sizeof(enderecoServidorHTTP)) < 0) { // conecta ao servidor
        perror("Erro ao conectar ao servidor HTTP");
        close(socketClienteHTTP);
        exit(EXIT_FAILURE);
    }

    char requisicaoHTTP[1024]; // buffer para requisicao HTTP
    snprintf(requisicaoHTTP, sizeof(requisicaoHTTP), // monta requisicao HTTP GET
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n", caminhoDoArquivoNoServidor, hostDoServidor);

    write(socketClienteHTTP, requisicaoHTTP, strlen(requisicaoHTTP)); // envia requisicao HTTP

    FILE *arquivoSaidaLocal = fopen(nomeArquivoSaidaLocal, "wb"); // abre arquivo local para escrita
    if(!arquivoSaidaLocal) {
        perror("Erro ao criar arquivo local de saida");
        close(socketClienteHTTP);
        exit(EXIT_FAILURE);
    }

    char bufferHTTP[BUFFER_TAMANHO]; // buffer para leitura de dados HTTP
    ssize_t bytesLidos; // bytes lidos do socket
    int64_t totalBytesBaixados = 0; // total de bytes baixados
    int64_t tamanhoTotalConteudo = -1; // tamanho total do conteudo (se conhecido)

    char cabecalhoHTTP[CABECALHO_TAMANHO] = {0}; // buffer para cabecalho HTTP
    size_t cabecalhoLido = 0; // bytes lidos no cabecalho

    // le cabecalho HTTP de forma segura (evita overflow)
    while((bytesLidos = read(socketClienteHTTP, bufferHTTP, BUFFER_TAMANHO)) > 0) {
        size_t aCopiar = bytesLidos;
        if(cabecalhoLido + aCopiar > sizeof(cabecalhoHTTP) - 1)
            aCopiar = sizeof(cabecalhoHTTP) - 1 - cabecalhoLido;
        memcpy(cabecalhoHTTP + cabecalhoLido, bufferHTTP, aCopiar);
        cabecalhoLido += aCopiar;
        cabecalhoHTTP[cabecalhoLido] = '\0';
        char *fimCabecalho = strstr(cabecalhoHTTP, "\r\n\r\n");
        if(fimCabecalho) { // encontrou fim do cabecalho
            size_t corpoIniciado = (fimCabecalho - cabecalhoHTTP) + 4;
            size_t corpoBytes = cabecalhoLido - corpoIniciado;
            if(corpoBytes > 0) { // se ja vieram bytes do corpo
                fwrite(cabecalhoHTTP + corpoIniciado, 1, corpoBytes, arquivoSaidaLocal);
                totalBytesBaixados += corpoBytes;
            }
            break;
        }
        if(cabecalhoLido >= sizeof(cabecalhoHTTP) - 1) // se ultrapassou cabecalho
            break;
    }

    if(strstr(cabecalhoHTTP, "200 OK") == NULL) { // verifica se resposta foi OK
        printf("Servidor retornou erro:\n%s\n", cabecalhoHTTP);
        fclose(arquivoSaidaLocal);
        close(socketClienteHTTP);
        exit(EXIT_FAILURE);
    }

    char *inicioContentLength = procuraSubstring(cabecalhoHTTP, "Content-Length:"); // procura tamanho do conteudo
    if(inicioContentLength) {
        inicioContentLength += 15; // pula seu comprimento no caso de ter encontrado substring
        while(*inicioContentLength == ' ') inicioContentLength++; // pula espacos
        tamanhoTotalConteudo = atoll(inicioContentLength);
    }

    while((bytesLidos = read(socketClienteHTTP, bufferHTTP, BUFFER_TAMANHO)) > 0) { // le corpo HTTP
        fwrite(bufferHTTP, 1, bytesLidos, arquivoSaidaLocal);
        totalBytesBaixados += bytesLidos;
        if(tamanhoTotalConteudo > 0) exibeProgressoDoDownload(totalBytesBaixados, tamanhoTotalConteudo);
    }

    printf("\nDownload concluido (%" PRId64 " bytes)\n", totalBytesBaixados);

    fclose(arquivoSaidaLocal); // fecha arquivo local
    close(socketClienteHTTP); // fecha socket
}

int main(int argc, char *argv[]) {

    if(argc < 2) { // precisa pelo menos da URL
        fprintf(stderr, "Uso: %s <URL>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char hostDoServidor[256];
    int portaDoServidor;
    char caminhoDoArquivoNoServidor[1024];
    extraiHostPortaECaminhoDaURL(argv[1], hostDoServidor, &portaDoServidor, caminhoDoArquivoNoServidor); // separa partes da URL

    const char *ultimoSlash = strrchr(caminhoDoArquivoNoServidor, '/'); // obtem nome do arquivo local
    char nomeArquivoSaidaLocal[1024];
    if(!ultimoSlash || strlen(ultimoSlash + 1) == 0)
        strcpy(nomeArquivoSaidaLocal, "index.html");
    else
        strcpy(nomeArquivoSaidaLocal, ultimoSlash + 1);

    printf("Conectando a %s:%d e baixando '%s'...\n", hostDoServidor, portaDoServidor, caminhoDoArquivoNoServidor);
    realizaDownloadHTTP(hostDoServidor, portaDoServidor, caminhoDoArquivoNoServidor, nomeArquivoSaidaLocal); // faz o download

    return 0;
}
