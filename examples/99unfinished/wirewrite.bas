5 PINM 13,1
10 C$="m":C$(2)=8
20 OPEN &7,C$
25 DWRITE 13,1
30 PRINT &7,"hello"
35 PRINT @S
40 DELAY 500
50 DWRITE 13,0
60 DELAY 500
70  GOTO 25