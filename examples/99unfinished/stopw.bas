100 REM "A little stopwatch using the 16*2 shield"
200 REM "The setup()"
210 D=0:T=0:U=0:S=0:C=0
220 PUT &2,12 : PRINT &2;"Time = ";
230 GOSUB 600
300 REM "The loop()"
320 GET &2,A
330 IF A=115AND C=0 THEN C=A:GOTO 400
340 IF C=115AND A=0 THEN C=0
350 IF A=108 THEN 500
360 DELAY 10
370 IF S=1 THEN T=MILLIS (10)-D
380 IF T<>U THEN U=T:GOSUB 600
390 GOTO 300
400 S=(S+1)%2:IF S=1 THEN D=MILLIS (10)
499 GOTO 300
500 END
600 @Y=0:@X=8:PRINT &2;"       ";
605 @Y=0:@X=8:PRINT &2;T;
610 RETURN 