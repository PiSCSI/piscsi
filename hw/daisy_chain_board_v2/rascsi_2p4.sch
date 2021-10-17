EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr USLetter 11000 8500
encoding utf-8
Sheet 1 2
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Wire Notes Line
	8300 4850 10200 4850
Wire Notes Line
	10200 2650 8300 2650
Text Notes 8400 4800 0    50   ~ 0
DB-25 SCSI Connector
Text Notes 3900 7900 0    50   ~ 0
This card include bus transceiver logic to allow a Raspberry \nPi to connect to a vintage Macintosh SCSI port. (It may \nwork with other systems as well)\n\nThis design is based upon GIMONS's Target/Initiator design\nhttp://retropc.net/gimons/rascsi/\n\nThis is the "FULLSPEC" version of the board that \ncan work as a SCSI target OR initiator\n\nThank you to everyone who has worked on this project!!
Text GLabel 8700 3850 0    50   BiDi ~ 0
C-D0
Text GLabel 9700 3850 2    50   BiDi ~ 0
C-D1
Text GLabel 9700 3950 2    50   BiDi ~ 0
C-D2
Text GLabel 8700 4050 0    50   BiDi ~ 0
C-D3
Text GLabel 9700 4050 2    50   BiDi ~ 0
C-D4
Text GLabel 8700 4150 0    50   BiDi ~ 0
C-D5
Text GLabel 8700 4250 0    50   BiDi ~ 0
C-D6
Text GLabel 8700 4350 0    50   BiDi ~ 0
C-D7
Text GLabel 9700 3750 2    50   BiDi ~ 0
C-DP
Text GLabel 9700 3450 2    50   BiDi ~ 0
C-ATN
Text GLabel 8700 3650 0    50   BiDi ~ 0
C-BSY
Text GLabel 8700 3550 0    50   BiDi ~ 0
C-ACK
Text GLabel 8700 3450 0    50   BiDi ~ 0
C-RST
Text GLabel 8700 3250 0    50   BiDi ~ 0
C-MSG
Text GLabel 9700 3650 2    50   BiDi ~ 0
C-SEL
Text GLabel 9700 3250 2    50   BiDi ~ 0
C-C_D
Text GLabel 8700 3150 0    50   BiDi ~ 0
C-REQ
Text GLabel 8700 3350 0    50   BiDi ~ 0
C-I_O
Text GLabel 9700 4250 2    50   BiDi ~ 0
TERMPOW
$Comp
L power:GND #PWR039
U 1 1 5F436924
P 9200 4650
F 0 "#PWR039" H 9200 4400 50  0001 C CNN
F 1 "GND" V 9205 4522 50  0000 R CNN
F 2 "" H 9200 4650 50  0001 C CNN
F 3 "" H 9200 4650 50  0001 C CNN
	1    9200 4650
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR042
U 1 1 5F460701
P 9700 3150
F 0 "#PWR042" H 9700 2900 50  0001 C CNN
F 1 "GND" V 9705 3022 50  0000 R CNN
F 2 "" H 9700 3150 50  0001 C CNN
F 3 "" H 9700 3150 50  0001 C CNN
	1    9700 3150
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR043
U 1 1 5F460CF4
P 9700 3350
F 0 "#PWR043" H 9700 3100 50  0001 C CNN
F 1 "GND" V 9705 3222 50  0000 R CNN
F 2 "" H 9700 3350 50  0001 C CNN
F 3 "" H 9700 3350 50  0001 C CNN
	1    9700 3350
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR044
U 1 1 5F460F07
P 9700 3550
F 0 "#PWR044" H 9700 3300 50  0001 C CNN
F 1 "GND" V 9705 3422 50  0000 R CNN
F 2 "" H 9700 3550 50  0001 C CNN
F 3 "" H 9700 3550 50  0001 C CNN
	1    9700 3550
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR035
U 1 1 5F46110C
P 8700 3750
F 0 "#PWR035" H 8700 3500 50  0001 C CNN
F 1 "GND" V 8705 3622 50  0000 R CNN
F 2 "" H 8700 3750 50  0001 C CNN
F 3 "" H 8700 3750 50  0001 C CNN
	1    8700 3750
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR036
U 1 1 5F4617B9
P 8700 3950
F 0 "#PWR036" H 8700 3700 50  0001 C CNN
F 1 "GND" V 8705 3822 50  0000 R CNN
F 2 "" H 8700 3950 50  0001 C CNN
F 3 "" H 8700 3950 50  0001 C CNN
	1    8700 3950
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR045
U 1 1 5F461986
P 9700 4150
F 0 "#PWR045" H 9700 3900 50  0001 C CNN
F 1 "GND" V 9705 4022 50  0000 R CNN
F 2 "" H 9700 4150 50  0001 C CNN
F 3 "" H 9700 4150 50  0001 C CNN
	1    9700 4150
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR038
U 1 1 5F462686
P 9200 2850
F 0 "#PWR038" H 9200 2600 50  0001 C CNN
F 1 "GND" V 9205 2722 50  0000 R CNN
F 2 "" H 9200 2850 50  0001 C CNN
F 3 "" H 9200 2850 50  0001 C CNN
	1    9200 2850
	0    -1   -1   0   
$EndComp
$Comp
L Mechanical:MountingHole H6
U 1 1 5EF89B2F
P 9800 5500
F 0 "H6" H 9900 5549 50  0000 L CNN
F 1 "Hole6" H 9900 5458 50  0000 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5" H 9800 5500 50  0001 C CNN
F 3 "DNP" H 9800 5500 50  0001 C CNN
F 4 "DNP" H 9800 5500 50  0001 C CNN "Description"
F 5 "" H 9800 5500 50  0001 C CNN "Height"
F 6 "DNP" H 9800 5500 50  0001 C CNN "LCSC"
F 7 "DNP" H 9800 5500 50  0001 C CNN "Manufacturer_Name"
F 8 "DNP" H 9800 5500 50  0001 C CNN "Manufacturer_Part_Number"
F 9 "" H 9800 5500 50  0001 C CNN "Mouser Part Number"
F 10 "" H 9800 5500 50  0001 C CNN "Mouser Price/Stock"
	1    9800 5500
	1    0    0    -1  
$EndComp
Wire Notes Line
	8300 5850 10200 5850
$Comp
L SamacSys_Parts:L717SDB25PA4CH4F J6
U 1 1 5FA017A4
P 9200 2850
F 0 "J6" V 9019 2850 50  0000 C CNN
F 1 "CONNFLY DB-25" V 9110 2850 50  0000 C CNN
F 2 "SamacSys_Parts:L717SDB25PA4CH4F" H 10850 3150 50  0001 L CNN
F 3 "https://datasheet.lcsc.com/szlcsc/1811081022_CONNFLY-Elec-DS1037-25FNAKT74-0CC_C77838.pdf" H 10850 3050 50  0001 L CNN
F 4 "D-Sub Receptacle,Female Socket 25 2 1.5A(Contact) Through Hole D-Sub Connectors RoHS" H 10850 2950 50  0001 L CNN "Description"
F 5 "" H 10850 2850 50  0001 L CNN "Height"
F 6 "" H 10850 2750 50  0001 L CNN "Mouser Part Number"
F 7 "" H 10850 2650 50  0001 L CNN "Mouser Price/Stock"
F 8 "CONNFLY Elec" H 10850 2550 50  0001 L CNN "Manufacturer_Name"
F 9 "DS1037-25FNAKT74-0CC" H 10850 2450 50  0001 L CNN "Manufacturer_Part_Number"
F 10 "C77838" V 9200 2850 50  0001 C CNN "LCSC"
	1    9200 2850
	0    -1   1    0   
$EndComp
Text Notes 6750 7760 0    87   ~ 17
RaSCSI - 68kmla Edition
Text Notes 9900 7890 0    79   ~ 16
2.4
Text Notes 7450 7870 0    59   ~ 12
28-Dec-2020
Wire Notes Line
	10200 5850 10200 2650
Wire Notes Line
	8300 5850 8300 2650
$Sheet
S 8300 5950 1900 550 
U 5FEBA38A
F0 "Connectors" 50
F1 "Connectors.sch" 50
$EndSheet
Text Notes 10200 5700 1    50   ~ 0
Mounting Holes
Wire Notes Line
	9250 5350 9250 5850
Wire Notes Line
	9250 5350 8300 5350
Text Notes 8600 5850 0    50   ~ 0
Images
$Comp
L SamacSys_Parts:Logo X3
U 1 1 5EFCD8D2
P 8550 5600
F 0 "X3" H 8400 5700 50  0000 L CNN
F 1 "Mac" H 8600 5700 50  0000 L CNN
F 2 "SamacSys_Parts:mac_happy_small" H 8550 5600 50  0001 C CNN
F 3 "N/A - Silkscreen" H 8550 5600 50  0001 C CNN
F 4 "" H 8550 5600 50  0001 C CNN "Height"
F 5 "N/A" H 8550 5600 50  0001 C CNN "LCSC"
F 6 "N/A" H 8550 5600 50  0001 C CNN "Manufacturer_Name"
F 7 "N/A" H 8550 5600 50  0001 C CNN "Manufacturer_Part_Number"
F 8 "" H 8550 5600 50  0001 C CNN "Mouser Part Number"
F 9 "" H 8550 5600 50  0001 C CNN "Mouser Price/Stock"
F 10 "N/A - Silkscreen" H 8550 5600 50  0001 C CNN "Description"
	1    8550 5600
	1    0    0    -1  
$EndComp
$Comp
L SamacSys_Parts:Logo X1
U 1 1 5EFCC51E
P 8550 5500
F 0 "X1" H 8400 5600 50  0000 L CNN
F 1 "Mac" H 8600 5600 50  0000 L CNN
F 2 "SamacSys_Parts:mac_happy_small" H 8550 5500 50  0001 C CNN
F 3 "N/A - Silkscreen" H 8550 5500 50  0001 C CNN
F 4 "" H 8550 5500 50  0001 C CNN "Height"
F 5 "N/A" H 8550 5500 50  0001 C CNN "LCSC"
F 6 "N/A" H 8550 5500 50  0001 C CNN "Manufacturer_Name"
F 7 "N/A" H 8550 5500 50  0001 C CNN "Manufacturer_Part_Number"
F 8 "" H 8550 5500 50  0001 C CNN "Mouser Part Number"
F 9 "" H 8550 5500 50  0001 C CNN "Mouser Price/Stock"
F 10 "N/A - Silkscreen" H 8550 5500 50  0001 C CNN "Description"
	1    8550 5500
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole H5
U 1 1 5EF89A1E
P 9800 5000
F 0 "H5" H 9900 5049 50  0000 L CNN
F 1 "Hole4" H 9900 4958 50  0000 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5" H 9800 5000 50  0001 C CNN
F 3 "DNP" H 9800 5000 50  0001 C CNN
F 4 "DNP" H 9800 5000 50  0001 C CNN "Description"
F 5 "" H 9800 5000 50  0001 C CNN "Height"
F 6 "DNP" H 9800 5000 50  0001 C CNN "LCSC"
F 7 "DNP" H 9800 5000 50  0001 C CNN "Manufacturer_Name"
F 8 "DNP" H 9800 5000 50  0001 C CNN "Manufacturer_Part_Number"
F 9 "" H 9800 5000 50  0001 C CNN "Mouser Part Number"
F 10 "" H 9800 5000 50  0001 C CNN "Mouser Price/Stock"
	1    9800 5000
	1    0    0    -1  
$EndComp
$Comp
L SamacSys_Parts:Logo X8
U 1 1 5FB668EF
P 8550 5550
F 0 "X8" H 8700 5600 50  0000 R CNN
F 1 "Pi" H 8500 5600 50  0000 R CNN
F 2 "SamacSys_Parts:scsi_logo" H 8550 5550 50  0001 C CNN
F 3 "N/A - Silkscreen" H 8550 5550 50  0001 C CNN
F 4 "" H 8550 5550 50  0001 C CNN "Height"
F 5 "N/A" H 8550 5550 50  0001 C CNN "LCSC"
F 6 "N/A" H 8550 5550 50  0001 C CNN "Manufacturer_Name"
F 7 "N/A" H 8550 5550 50  0001 C CNN "Manufacturer_Part_Number"
F 8 "" H 8550 5550 50  0001 C CNN "Mouser Part Number"
F 9 "" H 8550 5550 50  0001 C CNN "Mouser Price/Stock"
F 10 "N/A - Silkscreen" H 8550 5550 50  0001 C CNN "Description"
	1    8550 5550
	-1   0    0    1   
$EndComp
$EndSCHEMATC
