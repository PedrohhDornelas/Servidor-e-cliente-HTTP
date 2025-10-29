# Servidor e Cliente HTTP em C

Este código implementa um servidor HTTP simples em linguagem C que serve arquivos de um diretório, além de um cliente capaz de baixá-los.

Funcionalidades:

    - Atende requisições GET HTTP
    - Retorna arquivos binários ou de texto
    - Gera listagem automática de diretórios em HTML
    - Trata URLs com espaços
    - Compatível com qualquer navegador comum

Para usar o cliente, basta digitar

    make cliente
    ./cliente <URL>

Para usar o servidor, basta digitar
    
    make servidor

Note que é possível alterar o diretório do servidor através do M    akefile.
