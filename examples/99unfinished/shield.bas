10 PUT &2, 12
20 PRINT &2,"LCD shield test"
30 PRINT &2,"Key code: ";
40 @X=8:@Y=1
50 GET &2,A
60 IF A<>0 THEN PRINT &2,A;
70 DELAY 10
80 GOTO 40
