1 PRINT "TRAIN from 101 BASIC games"
2 PRINT "Ported to Stefan's BASIC in 2021"
3 PRINT
4 PRINT "TIME - SPEED DISTANCE EXERCISE": PRINT
5 DIM A$(8)
10 C=INT(RND(25))+40
15 D=INT(RND(15))+5
20 T=INT(RND(19))+20
25 PRINT "A CAR TRAVELING",C,"MPH CAN MAKE A CERTAIN TRIP IN"
30 PRINT D,"HOURS LESS THAN A TRAIN TRAVELING AT",T,"MPH."
35 PRINT "HOW LONG DOES THE TRIP TAKE BY CAR";
40 INPUT A
45 V=D*T/(C-T)
50 E=INT(ABS((V-A)*100/A)+0.5)
55 IF E>5 THEN 70
60 PRINT "GOOD! ANSWER WITHIN",E,"PERCENT."
65 GOTO 80
70 PRINT "SORRY.  YOU WERE OFF BY",E,"PERCENT."
80 PRINT "CORRECT ANSWER IS",V,"HOURS."
90 PRINT
95 PRINT "ANOTHER PROBLEM (YES OR NO)";
100 INPUT A$
105 PRINT
110 IF A$="YES" OR A$="yes" THEN 10
999 END