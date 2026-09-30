Тип фрагмента                   Условие                 Фильтр                  Пояснение
Первый фрагмент             Offset = 0, MF = 1      ip[6:2] & 0x1FFF = 0 and    Смещение нулевое, 
                                                    ip[6:2] & 0x2000 != 0       но флаг MF установлен   

Средний фрагмент            Offset > 0, MF = 1      ip[6:2] & 0x1FFF != 0 and   Смещение не нулевое
                                                    ip[6:2] & 0x2000 != 0       и флаг MF установлен

Последний фрагмент          Offset > 0, MF = 0      ip[6:2] & 0x1FFF != 0 and   Смещение не нулевое,
                                                    ip[6:2] & 0x2000 = 0        но флаг сброшен

Нефрагментированный пакет   Offset = 0, MF = 0      ip[6:2] & 0x3FFF = 0        Смещение нулевое и флаг сброшен

Команды для выполнения дз:

```
sudo tcpdump -i wlo1 -nn -s0 -w hw-2.pcap
```

1. icmp

```
ping -c 2 1.1.1.1
```

1. Смотрим TCP

```
tcpdump -nn -r hw-2.pcap 'tcp[13] & 0x12 == 0x02'
```

2. Смотрим DNS и сразу ищем по домену
   
```
tcpdump -nn -r hw-2.pcap 'port 53' | grep -i 'google.com'
```

```
23:51:05.922290 IP 10.10.206.125.46233 > 10.10.206.1.53: 43162+ A? www.google.com. (32)
```

3. Смотрим icmp

```
tcpdump -nn -r hw-2.pcap icmp
```

```
23:52:52.874797 IP 10.10.206.125 > 1.1.1.1: ICMP echo request, id 61682, seq 1, length 64
23:52:52.904500 IP 1.1.1.1 > 10.10.206.125: ICMP echo reply, id 61682, seq 1, length 64

```

1. Находим сессию SYN -> SYN-ACK -> ACK

```
tcpdump -nn -r hw-2.pcap 'host 10.10.206.125 and host 10.10.206.1 and port 46184 and port 53'
```

```
23:51:03.957249 IP 10.10.206.125.46184 > 10.10.206.1.53: Flags [S], seq 1306054169, win 64240, options [mss 1460,sackOK,TS val 3356231120 ecr 0,nop,wscale 10], length 0
23:51:03.964927 IP 10.10.206.1.53 > 10.10.206.125.46184: Flags [S.], seq 1711831231, ack 1306054170, win 4128, options [mss 1250], length 0
23:51:03.964954 IP 10.10.206.125.46184 > 10.10.206.1.53: Flags [.], ack 1, win 64240, length 0
```

4. 

```
tcpdump -nn -r hw-2.pcap | awk '{print $NF, $0}' | sort -nr | head -1
```

```
22284 23:51:34.105394 IP 95.163.52.80.443 > 10.10.206.125.60098: Flags [.], seq 1731434:1753718, ack 3201, win 2, options [nop,nop,TS val 704345061 ecr 1072774848], length 22284
```