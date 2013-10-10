#!/bin/bash

########################################################################
# MAIN                                                                 #
########################################################################
#Check if all arguments were given
if [ $# -ne 11 ]; then
  echo ""
  echo "ERROR - Usage: \"./uploadSrv.sh <slice> <hostFile> <AlgType> <quorSysFile> <serversNo> <portsNum> <failFreq> <wrtNo> <wOpFreq> <rdrNo> <rOpFreq>"
  echo " <failFreq> = [0,101]"
  echo ""
  echo "Example: ./uploadScr.sh uconn_exp hostsUconn.txt 1 25 qs-c25.txt 10001 80"
  echo ""
  # We have 11 arguments
else
  echo "Upload servers file"
  # Save arguments
  slice=$1              # Slice
  machines=$2           # Hosts File
  algType=$3            # Algorithm Type
  qsFile=$4		# Quorum System File
  srv=$5                # Number of servers
  ports=$6              # Beginning of Ports number
  fail=$7               # Fail Frequency
  wrt=$8		# Number of writers
  wrtFr=$9		# Frequency of operations for writers
  shift 2
  rdr=$8		# Number of readers
  rdrFr=$9		# Frequency of operationf for readers
  opNo=200
  ########################################################################
  # Choose algorithm to run
  if [ $algType -eq 0 ]; then
    echo "Simple Algorithm"
    server="sserver"
    writer="swriter"
    reader="sreader"
  else
    echo "Classic Algorithm (fast)"
    server="fserver"
    writer="fwriter"
    reader="freader"
  fi
  ########################################################################
  # Fill servers file
  name=(`echo $qsFile | tr '-' ' ' | cut -d ' ' -f 2`)
  echo $srv > srv-$name
  exec<$machines
  for (( i = 0; $i < $srv; i++ ));
  do
    read line
    if [ $((i%10)) -eq 0 ]; then
        exec<$machines
        read line
    fi
    host=$(echo $line);
    echo "$((i+1)) $((ports+i)) $host $fail" >> srv-$name
  done
  ########################################################################
  # Start session - Give password
  eval $(ssh-agent)
  ssh-add
  ########################################################################
  # Save nodes for servers
  declare -a hostnames

  exec<srv-$name
  read line     #ignore first line
  for (( i = 0; $i < $srv; i++ ));
  do
    read line
    hostnames[$i]=$(echo $line | cut -d " " -f3);
    echo "hostnames[$i] = ${hostnames[$i]}"
  done
  ########################################################################
  # Save rest of nodes
  # Read machines file, skip the first x hosts which are used for servers (1<x<11)
  exec<$machines
  servers=10
  i=0

  if [ $srv -lt $servers ]; then
    servers=$srv;
  fi

  while read line
  do
    i=$((i+1))
    if [ $i -le $servers ]; then
        continue;
    fi
    hostnames[$((srv-servers+i-1))]=$(echo $line);
    echo "-hostnames[$((srv-servers+i-1))] = ${hostnames[$((srv-servers+i-1))]}"
  done
  ########################################################################
  # Upload scripts for Servers
  j=0
  t=""
  for (( i = 0; $i < $srv; i++ ));
  do
    echo "./$server $((i+1)) $((ports+i)) $fail" > "run-h$((i+1)).sh"
    chmod 711 "run-h$((i+1)).sh"
    rsync -avz -e ssh ./run-h$((i+1)).sh $slice@${hostnames[$j]}:~/
    t+=" --tab -e 'bash -c \"echo ssh -l $slice -i ~/.ssh/id_rsa ${hostnames[$i]};\
       ssh -l $slice -i ~/.ssh/id_rsa ${hostnames[$i]};\bash\"'"
    j=$((j+1))
  done
  VAR="/usr/bin/gnome-terminal"$t
  eval $VAR
  ########################################################################
  # Upload file to all client hosts
  exec<$machines
  servers=10
  i=0
  
  if [ $srv -lt $servers ]; then
    servers=$srv;
  fi

  while read line
  do
    i=`expr $i + 1`;

    if [ $i -le $servers ]; then
        continue;
    fi
    host=$(echo $line);
    rsync -avz -e ssh ./srv-$name $slice@$host:~/
  done
  ########################################################################
  # Upload scripts for Writers
  # total is the maximum number of writers running in the same host
  # cntrs is a table with counters counting the number of writers for each host
  nodes=20

  wnodes=`expr $nodes - $servers`;
  if [ $wrt -lt $wnodes ]; then
    wnodes=$wrt;
  fi
  j=$srv
  t=""
  for (( i = $srv; $i < $((srv+wrt)); i++ ));
  do
    echo "./$writer $((i+1)) srv-$name $qsFile $opNo $wrtFr" > "run-w$((i+1)).sh"
    chmod 711 "run-w$((i+1)).sh"
    if [ $((j%35)) -eq 0 ]; then
        j=$srv
    fi
    rsync -avz -e ssh ./run-w$((i+1)).sh $slice@${hostnames[$j]}:~/
    t+=" --tab -e 'bash -c \"echo ssh -l $slice -i ~/.ssh/id_rsa ${hostnames[$j]};\
       ssh -l $slice -i ~/.ssh/id_rsa ${hostnames[$j]};\bash\"'"
    j=$((j+1))
  done
  VAR="/usr/bin/gnome-terminal"$t
  eval $VAR
  ########################################################################
  # Upload scripts for Readers
  rnodes=`expr $nodes - $servers`;
  if [ $rdr -lt $rnodes ]; then
    rnodes=$rdr;
  fi
  j=$srv
  t=""
  for (( i = $((srv+wrt)); $i < $((srv+wrt+rdr)); i++ ));
  do
    echo "./$reader $((i+1)) srv-$name $qsFile $opNo $rdrFr" > "run-r$((i+1)).sh"
    chmod 711 "run-r$((i+1)).sh"
    if [ $((j%35)) -eq 0 ]; then
        j=$srv
    fi
    rsync -avz -e ssh ./run-r$((i+1)).sh $slice@${hostnames[$j]}:~/
    t+=" --tab -e 'bash -c \"echo ssh -l $slice -i ~/.ssh/id_rsa ${hostnames[$j]};\
       ssh -l $slice -i ~/.ssh/id_rsa ${hostnames[$j]};\bash\"'"
    j=$((j+1))
  done
  VAR="/usr/bin/gnome-terminal"$t
  eval $VAR
  ########################################################################
fi