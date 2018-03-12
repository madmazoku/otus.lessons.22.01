echo ========= Build docker image
docker build -t otus.lessons.20.01 .
echo ========= Execute bulkmt
perl -e 'print "cmd$_\n" foreach 1..10;' | docker run --rm -i otus.lessons.20.01 bulkmt 4
echo ========= Remove docker image
docker rmi otus.lessons.20.01