all:
	gcc -c src/dropboxUtil.c -I include -o bin/dropboxUtil
	gcc src/dropboxClient.c bin/dropboxUtil -I include -o bin/dropboxClient
	gcc src/dropboxServer.c bin/dropboxUtil -I include -o bin/server/dropboxServer
	
leader_test:
	gcc -c src/dropboxUtil.c -I include -o bin/dropboxUtil
	gcc -c src/dropboxServer.c -I include -o bin/dropboxServer
	gcc -c src/list.c -I include -o bin/list
	gcc -c src/backup.c -I include -o bin/backup
	gcc -pthread src/leader.c bin/dropboxUtil bin/dropboxServer bin/list bin/backup -I include -o leader
