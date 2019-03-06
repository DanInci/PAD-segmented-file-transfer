



int main(int argv, char *argc[]) { // nume fisier, nr segmente
    // parinte: citeasca fiecare linie din servers.config, linie ce reprezinta adresa de conectare la un server
    //          interogheaza serverele si vede cate din ele sunt disponibile si se pune intr-o lista circulara
    //          verifica daca diminesiunea fisierului e mai mare decat nr de segmente
    //          calculeaza nr de bytes pe segment in mod egal, in functie de dimensiunea fisierului si nr de segmente (sa fie primul intreg superior rezultatului impartirii)
    //          creaza un proces pentru fiecare segment si imparte request-urile catre serverele disponibile intr-o maniera round robin
    //          uneste toate fisierele temporare
    //          sterge fisierele temporare
    //          verifica diminesiune fisier rezultat
    // fiu:     creaza conexiunea catre server
    //          trimite comanda de request de fiser la server
    //          citeste informatia din socket si o pune intr-un buffer
    //          scrie continutul buffer-ului intr-un fisier partial (nume_fiser-nr_segment)
    //          inchide conexiunea
}