ps -ef | grep recordServer | grep -v grep | awk '{print $2}' | xargs kill -9
ps -ef | grep recordMonitor | grep -v grep | awk '{print $2}' | xargs kill -9
