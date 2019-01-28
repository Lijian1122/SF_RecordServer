ps -ef | grep recordServer | grep -v grep | awk '{print $2}' | xargs kill -9
ipcs -q | awk 'NR > 3 {print "ipcrm -q", $2}' | sh