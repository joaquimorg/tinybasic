10 OPEN &8,"13040",1
20 OPEN &8,"13041",0
30 E=1
40 PINM AZERO ,0
50 H=575-AREAD (AZERO )
60 H=H*100/285
70 T=@T(7)/40
80 @E(E+1)=H:@E(E+2)=T
90 @E(E+3)=@T(2):@E(E+4)=@T(1)
100 E=(E+4)%400
110 REM "Wait"
120 FOR I=1 TO 900
130 IF AVAIL (8) GOSUB 300
140 DELAY 1000
150 NEXT 
160  GOTO 50
300 REM "Response"
310 INPUT &8,A$
320 IF A$(1,2)<>"RD"RETURN 
330 J=(A$(3)-32)*4+2:IF J<2 THEN J=2
340 B$="WG:":B$(4)=E/4+32:@E(1)=B$(4)
350 B$(5)=@E(J)+32:B$(6)=@E(J+1)+32
360 B$(7)=@E(J+2)+32:B$(8)=@E(J+3)+32
370 PRINT &8,B$
380 RETURN 
