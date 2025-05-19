# Admin Client-Server Project (Pop Samuel)

## Descriere generală
Această aplicație implementează un sistem Client-Server de administrare, utilizând socket-uri UNIX pe Linux.  
Proiectul suportă funcționalități complexe de control server, gestionare loguri, blocare IP, transfer fișiere și raportare.

## Funcționalități suportate
- **STATUS**: Afișează starea serverului și ora exactă.
- **SHUTDOWN**: Oprește serverul normal și loghează acțiunea.
- **LOGOUT**: Deconectează clientul admin fără să oprească serverul.
- **BLOCK_IP**: Blochează și loghează adrese IP.
- **KILL_SERVER**: Închide serverul forțat și loghează acțiunea.
- **GET_LOGS**: Afișează ultimele linii din fișierul de loguri server.
- **UPLOAD_FILE**: Trimite orice fișier la server.
- **DOWNLOAD_REPORT**: Descarcă un raport procesat de server.

## Structură proiect
- `admin_client.c`: codul sursă client admin
- `admin_server.c`: codul sursă server admin
- `Makefile`: fișier pentru compilare rapidă
- `/tmp/uploads/`: directorul unde serverul salvează fișierele primite
- `/tmp/admin_server.log`: log de activitate server
- `/tmp/blocked_ips.txt`: IP-uri blocate
- `/tmp/uploads_info.txt`: log upload fișiere

## Cerințe
- Linux OS (testat pe Ubuntu, WSL)
- GCC compiler

## Instrucțiuni de compilare
```bash
make

