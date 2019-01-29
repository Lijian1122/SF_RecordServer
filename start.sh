nohup valgrind --log-file=./valgrind_report.log --leak-check=full --show-leak-kinds=all --show-reachable=no --track-origins=yes  ./recordMonitor recordServer>&service.8081.log &
