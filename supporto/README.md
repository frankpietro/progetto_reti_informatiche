# progetto_reti_informatiche
Progetto su comunicazione UDP e TCP relativo al corso di Reti Informatiche, A.A. 2020-21, prof. Pistolesi

FILE PRESENTI
La cartella contenente i file necessari per compilazione ed esecuzione è strutturata così:
ds.c		peer.c		time.c		total_aggr.txt	util/		supporto/   makefile	exec.sh
Il file time.c implementa un server esterno che si occupa della gestione degli eventi relativi a date e orari, come il cambio dei registri alle ore 18. Il time server per ipotesi contiene gli aggregati di ogni singolo giorno dalla sua prima accensione (fissata al 2021/1/1).

Lo scambio di messaggi è solo di tipo UDP e si basa sul seguente algoritmo: ogni volta che un sender invia un pacchetto, ne attende uno di ack dal receiver; se dopo un tempo prefissato non lo ha ricevuto, lo invia nuovamente. Il receiver, dopo aver inviato l’ack, attende un tempo prefissato nel quale controlla che il sender non invii nuovamente il messaggio, ovvero abbia ricevuto correttamente l’ack.

I messaggi UDP sono tutti composti da un’intestazione identificativa di 8 caratteri, seguita da eventuali campi opzionali. La struttura di ogni pacchetto è nota a priori, perciò non c’è mai incertezza sul numero e sul significato di ulteriori campi. I messaggi di ack sono composti dalla sola intestazione. Il file supporto/codici.txt contiene tutte le associazioni tra messaggi, ack e loro utilizzo, divise in base ai ruoli di sender e receiver.

La struttura dei file principali è dettagliatamente riportata nei file supporto/schema_ds.txt, supporto/schema_peer.txt e supporto/schema_time.txt; vi sono riportati tutti i passaggi logici degli algoritmi implementati, e dove nel codice fossero chiamate funzioni di utilità ne è segnata l’operazione associata.

La cartella util/ contiene i seguenti file .c, con i relativi header .h:
msg.c --> scambio di messaggi;
peer_file.c --> gestione della topologia della rete;
retr_time.c --> gestione dell’orario corrente;
util_c.c --> funzioni di utilità per il peer;
util_s.c --> funzioni di utilità per il discovery server;
util_t.c --> funzioni di utilità per il time server.

Per ogni funzione è presente almeno un commento introduttivo che ne spiega il servizio offerto.

Il file total_aggr.txt contiene dati utili per osservare il corretto funzionamento della get già subito dopo l’avviamento del server.

TOPOLOGIA
I peer sono organizzati con topologia ad anello, ordinato per numero di porta: a ogni chiamata della start, il server inserisce nel punto giusto il peer richiedente e aggiorna i vicini degli eventuali peer a cui è cambiata la lista di prossimità. Un peer ha 0 vicini se è da solo, 1 se ci sono due peer collegati e 2 in ogni altro caso.

Quando un peer richiede di uscire, le liste di prossimità vengono aggiornate in modo analogo.

Il server mantiene costantemente aggiornato un file contenente l’anello dei peer, dal quale ricava le informazioni per eseguire le funzioni showpeers e showneighbor.

FUNZIONAMENTO E CARATTERISTICHE
Per una spiegazione dettagliata degli algoritmi implementati si rimanda ai file di schema. L’unico scostamento rispetto alle specifiche riguarda la get: per essa si è scelto di concedere l’opportunità di visualizzare i dati del giorno in corso. Questo ha permesso di scrivere un corposo algoritmo di query flooding, senza interventi da parte del server, per il calcolo dell’aggregato del giorno in corso, e allo stesso tempo di eliminare buona parte del carico sulla rete: si è scelto di cancellare giorno per giorno i registri con le singole entrate e mantenerne uno solo con lo storico degli aggregati.

Per implementare la chiusura dei registri alle 18 e il loro cambio si è scelto di introdurre un time server che pochi minuti prima delle 18 blocchi l’input da stdin per i peer ed esegua una raccolta e condensazione dei dati giornalieri. Per il cambio dei registri si è scelto di nominarli con una stringa che comprendesse la data corrente, incrementata alle 18 e non alle 24.

La gestione dei socket è stata mascherata dalle funzioni send_UDP, recv_UDP e ack_UDP, che consentono un utilizzo di alto livello delle funzionalità per lo scambio di messaggi.

VULNERABILITÀ
Sono state inserite alcune situazioni di mutua esclusione per garantire la consistenza del database distribuito. In particolare, mentre un peer esegue una get nessun altro può eseguire operazioni; mentre un peer esegue una stop, agli altri è impedito di eseguire un’altra stop o una start. Il problema può comunque verificarsi nel caso le funzioni siano lanciate pressoché in contemporanea su peer diversi. Questo è stato parzialmente aggirato rendendo molto breve (nell’ordine delle decine di millisecondi) il tempo di attesa dopo un ack, in modo da velocizzare l’esecuzione di tutte le funzioni e ridurre il rischio di deadlock. Ciononostante, difficilmente il server sopravviverebbe a un attacco di tipo DoS, oppure a un carico di peer nell’ordine delle centinaia o superiore.

Un altro problema è rappresentato dalla consistenza dei dati in caso di eventuale spegnimento del server, perché sarebbe difficile, oppure molto oneroso dal punto di vista computazionale, mantenere una organizzazione sensata del database distribuito, dato che non si saprebbe se e quando i peer dovessero riconnettersi. Inoltre, è fondamentale che entrambi i server siano accesi al momento del cambio dei registri.

Per questo motivo, sono state implementate tutte le funzioni necessarie a realizzare una corretta uscita del server, ma la consistenza dei dati si può mantenere solo se all’uscita segue una riaccensione ordinata dei due server e di tutti i peer connessi al momento del’uscita, senza che nel frattempo siano compiute operazioni di trasferimento dati.

Lo spegnimento del server, dunque, è stato considerato un evento imprevisto da gestire con cautela (magari con l’ausilio di altri programmi).