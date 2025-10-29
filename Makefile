.PHONY: servidor cliente clean

servidor:
	gcc -w servidor.c -o servidor
	clear
	./servidor /home/pedro/sv/Servidor-e-cliente-HTTP/arquivos

cliente:
	gcc cliente.c -o cliente
	clear
	printf "\nUso: ./cliente <URL>\n\n"

clean:
	rm -f cliente servidor
	clear
