.PHONY: servidor cliente clean

servidor:
	gcc -w servidor.c -o servidor
	clear
	./servidor /home/pedro/servidor/

cliente:
	gcc cliente.c -o cliente
	clear
	printf "\nUso: ./cliente <URL>\n\n"

clean:
	rm -f cliente servidor
	clear
