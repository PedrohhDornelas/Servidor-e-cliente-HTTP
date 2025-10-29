// Baseado no codigo em https://andy2903-alp.medium.com/criando-um-servidor-http-simples-em-c-8fcaf5d794c3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> // funcoes para sockets
#include <sys/stat.h> // permite verificar se arquivo ou diretorio
#include <dirent.h> // manipulacao de diretorio
#include <ctype.h> // isxdigit

#define PORT 8080 // porta padrao do servidor
#define Buffer_TAMANHO 1024 // tamanho do buffer para requisicoes

void trataURL(char *dst, const char *src) { // decodificar URLs
    char a, b;
    while(*src) {
        if((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if(a >= 'a') a -= 'a' - 'A';
            if(a >= 'A') a -= ('A' - 10); else a -= '0';
            if(b >= 'a') b -= 'a' - 'A';
            if(b >= 'A') b -= ('A' - 10); else b -= '0';
            *dst++ = 16 * a + b; // converte valor hexadecimal
            src += 3; // pula os 3 caracteres
        } else if(*src == '+') { // trata espacos
            *dst++ = ' '; // converte + para espaco
            src++; // pula o +
        } else {
            *dst++ = *src++; // copia caracter normal
        }
    }
    *dst = '\0';
}

void enviaCabecalho(int socketCliente, off_t tamanhoConteudo, const char *tipoConteudo) { // envia cabecalho HTTP
    char bufferCabecalho[256]; // buffer para cabecalho
    snprintf(bufferCabecalho, sizeof(bufferCabecalho), // monta cabecalho HTTP
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %lld\r\n"
            "\r\n", tipoConteudo, (long long)tamanhoConteudo);
    write(socketCliente, bufferCabecalho, strlen(bufferCabecalho)); // envia cabecalho
}

void enviaConteudo(int socketCliente, const char *caminhoArquivo) { // envia arquivo

    FILE *f = fopen(caminhoArquivo, "rb"); // abre binario para poder aceitar qualquer tipo
    if(!f) {
        const char *naoEncontrado = "HTTP/1.1 404 Not Found\r\n\r\nFile not found (404)";
        write(socketCliente, naoEncontrado, strlen(naoEncontrado)); // envia mensagem de erro
        return;
    }

    fseeko(f, 0, SEEK_END); // percorre arquivo para encontrar tamanho (usa off_t)
    off_t tamanho = ftello(f); // ftello pega a posicao atual (= tamanho)
    if(tamanho < 0) tamanho = 0; // evita tamanho negativo em erro de ftello
    fseeko(f, 0, SEEK_SET); // volta para o inicio do arquivo

    enviaCabecalho(socketCliente, tamanho, "application/octet-stream"); // envia cabecalho
    char bufferArquivo[Buffer_TAMANHO]; // buffer para leitura do arquivo
    size_t bytes; // bytes lidos
    while((bytes = fread(bufferArquivo, 1, Buffer_TAMANHO, f)) > 0) { // le arquivo em pedacos
        write(socketCliente, bufferArquivo, bytes); // envia pedaco lido
    }

    fclose(f);
}

void listaDiretorio(int socketCliente, const char *caminhoArquivo, const char *caminhoURL) { // lista diretorios clicaveis
    DIR *d = opendir(caminhoArquivo); // abre o diretorio
    if(!d) {
        const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\nDirectory not found";
        write(socketCliente, not_found, strlen(not_found)); // envia mensagem de erro
        return;
    }

    char bufferCorpo[Buffer_TAMANHO * 8] = {0}; // buffer para o corpo HTML
    strcat(bufferCorpo, "<html><body><ul>"); // concatena inicio do HTML
    struct dirent *entrada; // entrada para diretorio
    while((entrada = readdir(d)) != NULL) { // le cada entrada do diretorio
        if(strcmp(entrada->d_name, ".") == 0) 
            continue; // pula pontos

        // evita estouro de buffer
        size_t espacoNecessario = strlen(bufferCorpo) + strlen(caminhoURL) + strlen(entrada->d_name) + 128;
        if(espacoNecessario >= sizeof(bufferCorpo))
            break;

        strcat(bufferCorpo, "<li><a href=\""); // concatena inicio do link
        strcat(bufferCorpo, caminhoURL); // concatena caminho URL relativo
        if(strcmp(caminhoURL, "/") != 0) 
            strcat(bufferCorpo, "/"); // nao raiz deve concatenar barra
        strcat(bufferCorpo, entrada->d_name); // concatena nome do arquivo
        strcat(bufferCorpo, "\">"); // fim do link clicavel
        strcat(bufferCorpo, entrada->d_name); // nome visivel
        strcat(bufferCorpo, "</a></li>"); // fim do link
    }
    strcat(bufferCorpo, "</ul></body></html>"); // fim do HTML
    closedir(d);

    enviaCabecalho(socketCliente, strlen(bufferCorpo), "text/html"); // envia cabecalho para listagem de diretorio
    write(socketCliente, bufferCorpo, strlen(bufferCorpo)); // envia corpo HTML
}

void trataGET(int novoSocketCliente, const char *diretorioRaiz, const char *caminhoArquivo) { // trata requisicao GET
    char caminhoCompleto[2048]; // buffer para caminho completo
    snprintf(caminhoCompleto, sizeof(caminhoCompleto), "%s%s", diretorioRaiz, caminhoArquivo); // monta caminho completo

    struct stat st; // estrutura para informacoes do arquivo
    if(stat(caminhoCompleto, &st) == 0) { // verifica se o arquivo/diretorio existe
        if(S_ISREG(st.st_mode)) {
            enviaConteudo(novoSocketCliente, caminhoCompleto); // envia caso arquivo
        } else if(S_ISDIR(st.st_mode)) { // se for diretorio
            char caminhoIndex[2048]; // buffer para index.html
            snprintf(caminhoIndex, sizeof(caminhoIndex), "%s/index.html", caminhoCompleto); // monta caminho para index.html
            if(stat(caminhoIndex, &st) == 0) {
                enviaConteudo(novoSocketCliente, caminhoIndex); // envia index.html se ele existir
            } else {
                listaDiretorio(novoSocketCliente, caminhoCompleto, caminhoArquivo); // senao lista o diretorio
            }
        }
    } else {
        const char *naoENcontrado = "HTTP/1.1 404 Not Found\r\n\r\nFile not found (404)"; 
        write(novoSocketCliente, naoENcontrado, strlen(naoENcontrado)); // envia mensagem de erro
    }
}

void trataConexao(int novoSocketCliente, const char *diretorioRaiz, char *bufferPreenchido) {
    printf("Requisicao recebida:\n%s\n", bufferPreenchido); // esse buffer ja tem a requisicao lida

    char bufferMetodo[8]; // buffer para metodo HTTP
    char caminhoArquivo[1024] = {0}; // caminho para o arquivo requisitado
    sscanf(bufferPreenchido, "%7s %1023s", bufferMetodo, caminhoArquivo); // busca metodo e caminho (com limites seguros)
    if(strcmp(bufferMetodo, "GET") != 0) { // suporta apenas GET
        const char *naoSuportado = "HTTP/1.1 405 Method Not Allowed\r\n\r\nOnly GET supported";
        write(novoSocketCliente, naoSuportado, strlen(naoSuportado));
        return;
    }

    char caminhoDecodificado[1024]; // buffer para caminho decodificado
    trataURL(caminhoDecodificado, caminhoArquivo); // decodifica URL para lidar com espaços e caracteres especiais
    trataGET(novoSocketCliente, diretorioRaiz, caminhoDecodificado); // trata requisicao GET
}


int main(int argc, char *argv[]) {

    if(argc < 2) { // < 2 significa que nao incluiu o diretorio raiz
        fprintf(stderr, "Uso: %s <diretorio>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *diretorioRaiz = argv[1]; // diretorio raiz do servidor

    struct sockaddr_in enderecoSocketServidor;
    char bufferRequisicoes[Buffer_TAMANHO] = {0};

    int fileDescriptorServer = socket(AF_INET, SOCK_STREAM, 0); // cria socket // AF_INET ipv4, SOCK_STREAM tcp
    if(fileDescriptorServer < 0) { // < 0 significa que o socket nao foi criado
        perror("Erro ao criar o socket");
        exit(EXIT_FAILURE);
    }

    enderecoSocketServidor.sin_family = AF_INET; // configura para endereco ipv4
    enderecoSocketServidor.sin_addr.s_addr = INADDR_ANY; // escuta todo ip disponivel
    enderecoSocketServidor.sin_port = htons(PORT); // configura porta para rede
    int associacao = bind(fileDescriptorServer, (struct sockaddr *)&enderecoSocketServidor, sizeof(enderecoSocketServidor)); // associa o socket ao endereco e porta
    if(associacao < 0) { // < 0 significa que o socket nao foi associado
        perror("Erro ao associar o socket ao endereco ou porta");
        close(fileDescriptorServer);
        exit(EXIT_FAILURE);
    }

    int escuta = listen(fileDescriptorServer, 3); // 3 equivale ao maximo de conexoes na fila
    if(escuta < 0) { // < 0 significa que o socket nao esta em modo de escuta
        perror("Erro ao escutar");
        close(fileDescriptorServer);
        exit(EXIT_FAILURE);
    }

    printf("Servidor HTTP iniciado na porta %d...\n", PORT);

    while(1) { // loop para aceitar conexoes
        struct sockaddr_in enderecoCliente;
        socklen_t tamanhoEndereco = sizeof(enderecoCliente);
        int novoSocketCliente = accept(fileDescriptorServer, (struct sockaddr *)&enderecoCliente, &tamanhoEndereco); // aceita conexao
        if(novoSocketCliente < 0) { // < 0 significa que a conexao nao foi aceita
            perror("Erro ao aceitar conexao");
            continue;
        }

        memset(bufferRequisicoes, 0, Buffer_TAMANHO); // limpa buffer antes de ler nova requisicao
        ssize_t lidos = read(novoSocketCliente, bufferRequisicoes, Buffer_TAMANHO - 1); // le a requisicao do cliente
        if(lidos > 0)
            bufferRequisicoes[lidos] = '\0'; // garante string terminada
        trataConexao(novoSocketCliente, diretorioRaiz, bufferRequisicoes); // trata a conexao aceita

        close(novoSocketCliente); // fecha o socket do cliente
    }

    close(fileDescriptorServer);

    return 0;
}
