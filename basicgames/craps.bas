5 PRINT "CRAPS"
12 PRINT:PRINT:PRINT
15 LET R=0
20 PRINT"2,3,12 ARE LOSERS; 4,5,6,8,9,10 ARE POINTS; 7,11 ARE NATURAL WINNERS."
21 LET T=1
22 PRINT "PICK A NUMBER AND INPUT TO ROLL DICE";
23 INPUT Z
24 LET X=(RND(2))
25 LET T =T+1
26 IF T<=Z THEN 24
27 PRINT"INPUT THE AMOUNT OF YOUR WAGER.";
28 INPUT F
30 PRINT "I WILL NOW THROW THE DICE"
40 LET E=RND(7)
41 LET S=RND(7)
42 LET X=E+S
50 IF X=7 THEN 180 
55 IF X=11 THEN 180
60 IF X=1 THEN 40
62 IF X=2 THEN 195
65 IF X=0 THEN 40
70 IF X=2 THEN 200
80 IF X=3 THEN 200
90 IF X=12 THEN 200
125 IF X=5 THEN 220
130 IF X =6 THEN 220
140 IF X=8 THEN 220
150 IF X=9 THEN 220
160 IF X =10 THEN 220
170 IF X=4 THEN 220
180 PRINT X "- NATURAL....A WINNER!!!!"
185 PRINT X"PAYS EVEN MONEY, YOU WIN"F"DOLLARS"
190 GOTO 210
195 PRINT X"- SNAKE EYES....YOU LOSE."
196 PRINT "YOU LOSE"F "DOLLARS."
197 LET F=0-F
198 GOTO 210
200 PRINT X " - CRAPS...YOU LOSE."
205 PRINT "YOU LOSE"F"DOLLARS."
206 LET F=0-F
210 LET R= R+F
211 GOTO 320
220 PRINT X "IS THE POINT. I WILL ROLL AGAIN"
230 LET H=RND(7)
231 LET Q=RND(7)
232 LET O=H+Q
240 IF O=1 THEN 230
250 IF O=7 THEN 290
255 IF O=0 THEN 230
260 IF O=X THEN 310
270 PRINT O " - NO POINT. I WILL ROLL AGAIN"
280 GOTO 230
290 PRINT O "- CRAPS. YOU LOSE."
291 PRINT "YOU LOSE $"F
292 F=0-F
293 GOTO 210
300 GOTO 320
310 PRINT X; "- A WINNER.........CONGRATS!!!!!!!!"
311 PRINT X; "AT 2 TO 1 ODDS PAYS YOU...LET ME SEE..."2*F"DOLLARS"
312 LET F=2*F
313 GOTO 210
320 PRINT " IF YOU WANT TO PLAY AGAIN PRINT 5 IF NOT PRINT 2";
330 INPUT M
331 IF R<0 THEN 334
332 IF R>0 THEN 336
333 IF R=0 THEN 338
334 PRINT "YOU ARE NOW UNDER $";-R
335 GOTO 340
336 PRINT "YOU ARE NOW AHEAD $";R
337 GOTO 340
338 PRINT "YOU ARE NOW EVEN AT 0"
340 IF M=5 THEN 27
341 IF R<0 THEN 350
342 IF R>0 THEN 353
343 IF R=0 THEN 355
350 PRINT"TOO BAD, YOU ARE IN THE HOLE. COME AGAIN."
351 GOTO 360
353 PRINT"CONGRATULATIONS---YOU CAME OUT A WINNER. COME AGAIN!"
354 GOTO 360
355 PRINT"CONGRATULATIONS---YOU CAME OUT EVEN, NOT BAD FOR AN AMATEUR"
360 END
