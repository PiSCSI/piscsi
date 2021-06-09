EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr USLetter 11000 8500
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Connector_Generic:Conn_02x25_Odd_Even J7
U 1 1 5EF63F70
P 3550 2100
F 0 "J7" H 3600 675 50  0000 C CNN
F 1 "Conn_02x25_Odd_Even" H 3600 766 50  0000 C CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_2x25_P2.54mm_Vertical" H 3550 2100 50  0001 C CNN
F 3 "~" H 3550 2100 50  0001 C CNN
F 4 "C40024" H 3550 2100 50  0001 C CNN "LCSC"
	1    3550 2100
	0    -1   -1   0   
$EndComp
Wire Wire Line
	2350 2400 2350 2300
Wire Wire Line
	2350 2400 2450 2400
Wire Wire Line
	2450 2400 2450 2300
Wire Wire Line
	2450 2400 2550 2400
Wire Wire Line
	2550 2400 2550 2300
Connection ~ 2450 2400
Wire Wire Line
	2550 2400 2650 2400
Wire Wire Line
	2650 2400 2650 2300
Connection ~ 2550 2400
Wire Wire Line
	2650 2400 2750 2400
Wire Wire Line
	2750 2400 2750 2300
Connection ~ 2650 2400
Wire Wire Line
	2750 2400 2850 2400
Wire Wire Line
	2850 2400 2850 2300
Connection ~ 2750 2400
Wire Wire Line
	2850 2400 2950 2400
Wire Wire Line
	2950 2400 2950 2300
Connection ~ 2850 2400
Wire Wire Line
	2950 2400 3050 2400
Wire Wire Line
	3050 2400 3050 2300
Connection ~ 2950 2400
Wire Wire Line
	3050 2400 3150 2400
Wire Wire Line
	3150 2400 3150 2300
Connection ~ 3050 2400
Wire Wire Line
	3150 2400 3250 2400
Wire Wire Line
	3250 2400 3250 2300
Connection ~ 3150 2400
Wire Wire Line
	3250 2400 3350 2400
Wire Wire Line
	3350 2400 3350 2300
Connection ~ 3250 2400
Wire Wire Line
	3350 2400 3450 2400
Wire Wire Line
	3450 2400 3450 2300
Connection ~ 3350 2400
Wire Wire Line
	3450 2400 3650 2400
Wire Wire Line
	3650 2400 3650 2300
Connection ~ 3450 2400
Wire Wire Line
	3650 2400 3750 2400
Wire Wire Line
	3750 2400 3750 2300
Connection ~ 3650 2400
Wire Wire Line
	3750 2400 3850 2400
Wire Wire Line
	3850 2400 3850 2300
Connection ~ 3750 2400
Wire Wire Line
	3850 2400 3950 2400
Wire Wire Line
	3950 2400 3950 2300
Connection ~ 3850 2400
Wire Wire Line
	3950 2400 4050 2400
Wire Wire Line
	4050 2400 4050 2300
Connection ~ 3950 2400
Wire Wire Line
	4050 2400 4150 2400
Wire Wire Line
	4150 2400 4150 2300
Connection ~ 4050 2400
Wire Wire Line
	4150 2400 4250 2400
Wire Wire Line
	4250 2400 4250 2300
Connection ~ 4150 2400
Wire Wire Line
	4250 2400 4350 2400
Wire Wire Line
	4350 2400 4350 2300
Connection ~ 4250 2400
Wire Wire Line
	4350 2400 4450 2400
Wire Wire Line
	4450 2400 4450 2300
Connection ~ 4350 2400
Wire Wire Line
	4450 2400 4550 2400
Wire Wire Line
	4550 2400 4550 2300
Connection ~ 4450 2400
Wire Wire Line
	4550 2400 4650 2400
Wire Wire Line
	4650 2400 4650 2300
Connection ~ 4550 2400
Wire Wire Line
	4650 2400 4750 2400
Wire Wire Line
	4750 2400 4750 2300
Connection ~ 4650 2400
Text GLabel 2350 1800 1    50   BiDi ~ 0
C-D0
Text GLabel 2450 1800 1    50   BiDi ~ 0
C-D1
Text GLabel 2550 1800 1    50   BiDi ~ 0
C-D2
Text GLabel 2650 1800 1    50   BiDi ~ 0
C-D3
Text GLabel 2750 1800 1    50   BiDi ~ 0
C-D4
Text GLabel 2850 1800 1    50   BiDi ~ 0
C-D5
Text GLabel 2950 1800 1    50   BiDi ~ 0
C-D6
Text GLabel 3050 1800 1    50   BiDi ~ 0
C-D7
Text GLabel 3150 1800 1    50   BiDi ~ 0
C-DP
Text GLabel 3250 1800 1    50   BiDi ~ 0
GND
Text GLabel 3350 1800 1    50   BiDi ~ 0
GND
Text GLabel 3450 1800 1    50   BiDi ~ 0
GND
Text GLabel 3650 1800 1    50   BiDi ~ 0
GND
Text GLabel 3750 1800 1    50   BiDi ~ 0
GND
Text GLabel 3850 1800 1    50   BiDi ~ 0
C-ATN
Text GLabel 4050 1800 1    50   BiDi ~ 0
C-BSY
Text GLabel 4150 1800 1    50   BiDi ~ 0
C-ACK
Text GLabel 4250 1800 1    50   BiDi ~ 0
C-RST
Text GLabel 4350 1800 1    50   BiDi ~ 0
C-MSG
Text GLabel 4450 1800 1    50   BiDi ~ 0
C-SEL
Text GLabel 4550 1800 1    50   BiDi ~ 0
C-C_D
Text GLabel 4650 1800 1    50   BiDi ~ 0
C-REQ
Text GLabel 4750 1800 1    50   BiDi ~ 0
C-I_O
NoConn ~ 3550 2300
NoConn ~ 3950 1800
Wire Notes Line
	2000 4900 3900 4900
Wire Notes Line
	3900 6450 3900 2700
Text Notes 2100 4850 0    50   ~ 0
DB-25 SCSI Connector
Text Notes 2150 2650 0    50   ~ 0
SCSI Ribbon Cable
Wire Notes Line
	2000 1300 5250 1300
$Comp
L power:GND #PWR015
U 1 1 5F3086C0
P 4750 2400
F 0 "#PWR015" H 4750 2150 50  0001 C CNN
F 1 "GND" H 4755 2227 50  0000 C CNN
F 2 "" H 4750 2400 50  0001 C CNN
F 3 "" H 4750 2400 50  0001 C CNN
	1    4750 2400
	1    0    0    -1  
$EndComp
Connection ~ 4750 2400
Text GLabel 2400 3900 0    50   BiDi ~ 0
C-D0
Text GLabel 3400 3900 2    50   BiDi ~ 0
C-D1
Text GLabel 3400 4000 2    50   BiDi ~ 0
C-D2
Text GLabel 2400 4100 0    50   BiDi ~ 0
C-D3
Text GLabel 3400 4100 2    50   BiDi ~ 0
C-D4
Text GLabel 2400 4200 0    50   BiDi ~ 0
C-D5
Text GLabel 2400 4300 0    50   BiDi ~ 0
C-D6
Text GLabel 2400 4400 0    50   BiDi ~ 0
C-D7
Text GLabel 3400 3800 2    50   BiDi ~ 0
C-DP
Text GLabel 3400 3500 2    50   BiDi ~ 0
C-ATN
Text GLabel 2400 3700 0    50   BiDi ~ 0
C-BSY
Text GLabel 2400 3600 0    50   BiDi ~ 0
C-ACK
Text GLabel 2400 3500 0    50   BiDi ~ 0
C-RST
Text GLabel 2400 3300 0    50   BiDi ~ 0
C-MSG
Text GLabel 3400 3700 2    50   BiDi ~ 0
C-SEL
Text GLabel 3400 3300 2    50   BiDi ~ 0
C-C_D
Text GLabel 2400 3200 0    50   BiDi ~ 0
C-REQ
Text GLabel 2400 3400 0    50   BiDi ~ 0
C-I_O
Text GLabel 3400 4300 2    50   BiDi ~ 0
TERMPOW
$Comp
L power:GND #PWR039
U 1 1 5F436924
P 2900 4700
F 0 "#PWR039" H 2900 4450 50  0001 C CNN
F 1 "GND" V 2905 4572 50  0000 R CNN
F 2 "" H 2900 4700 50  0001 C CNN
F 3 "" H 2900 4700 50  0001 C CNN
	1    2900 4700
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR042
U 1 1 5F460701
P 3400 3200
F 0 "#PWR042" H 3400 2950 50  0001 C CNN
F 1 "GND" V 3405 3072 50  0000 R CNN
F 2 "" H 3400 3200 50  0001 C CNN
F 3 "" H 3400 3200 50  0001 C CNN
	1    3400 3200
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR043
U 1 1 5F460CF4
P 3400 3400
F 0 "#PWR043" H 3400 3150 50  0001 C CNN
F 1 "GND" V 3405 3272 50  0000 R CNN
F 2 "" H 3400 3400 50  0001 C CNN
F 3 "" H 3400 3400 50  0001 C CNN
	1    3400 3400
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR044
U 1 1 5F460F07
P 3400 3600
F 0 "#PWR044" H 3400 3350 50  0001 C CNN
F 1 "GND" V 3405 3472 50  0000 R CNN
F 2 "" H 3400 3600 50  0001 C CNN
F 3 "" H 3400 3600 50  0001 C CNN
	1    3400 3600
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR035
U 1 1 5F46110C
P 2400 3800
F 0 "#PWR035" H 2400 3550 50  0001 C CNN
F 1 "GND" V 2405 3672 50  0000 R CNN
F 2 "" H 2400 3800 50  0001 C CNN
F 3 "" H 2400 3800 50  0001 C CNN
	1    2400 3800
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR036
U 1 1 5F4617B9
P 2400 4000
F 0 "#PWR036" H 2400 3750 50  0001 C CNN
F 1 "GND" V 2405 3872 50  0000 R CNN
F 2 "" H 2400 4000 50  0001 C CNN
F 3 "" H 2400 4000 50  0001 C CNN
	1    2400 4000
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR045
U 1 1 5F461986
P 3400 4200
F 0 "#PWR045" H 3400 3950 50  0001 C CNN
F 1 "GND" V 3405 4072 50  0000 R CNN
F 2 "" H 3400 4200 50  0001 C CNN
F 3 "" H 3400 4200 50  0001 C CNN
	1    3400 4200
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR038
U 1 1 5F462686
P 2900 2900
F 0 "#PWR038" H 2900 2650 50  0001 C CNN
F 1 "GND" V 2905 2772 50  0000 R CNN
F 2 "" H 2900 2900 50  0001 C CNN
F 3 "" H 2900 2900 50  0001 C CNN
	1    2900 2900
	0    -1   -1   0   
$EndComp
$Comp
L Mechanical:MountingHole_Pad H5
U 1 1 5EF89A1E
P 2450 5300
F 0 "H5" H 2550 5349 50  0000 L CNN
F 1 "Hole4" H 2550 5258 50  0000 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5_Pad_Via" H 2450 5300 50  0001 C CNN
F 3 "~" H 2450 5300 50  0001 C CNN
	1    2450 5300
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole_Pad H6
U 1 1 5EF89B2F
P 2850 5300
F 0 "H6" H 2950 5349 50  0000 L CNN
F 1 "Hole6" H 2950 5258 50  0000 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5_Pad_Via" H 2850 5300 50  0001 C CNN
F 3 "~" H 2850 5300 50  0001 C CNN
	1    2850 5300
	1    0    0    -1  
$EndComp
$Comp
L SamacSys_Parts:Logo X1
U 1 1 5EFCC51E
P 2250 6100
F 0 "X1" H 2100 6200 50  0000 L CNN
F 1 "Mac" H 2300 6200 50  0000 L CNN
F 2 "SamacSys_Parts:mac_happy_small" H 2250 6100 50  0001 C CNN
F 3 "" H 2250 6100 50  0001 C CNN
	1    2250 6100
	1    0    0    -1  
$EndComp
$Comp
L SamacSys_Parts:Logo X2
U 1 1 5EFCD6CA
P 2250 6200
F 0 "X2" H 2100 6300 50  0000 L CNN
F 1 "Dogcow" H 2300 6300 50  0000 L CNN
F 2 "SamacSys_Parts:dogcow" H 2250 6200 50  0001 C CNN
F 3 "" H 2250 6200 50  0001 C CNN
	1    2250 6200
	1    0    0    -1  
$EndComp
$Comp
L SamacSys_Parts:Logo X3
U 1 1 5EFCD8D2
P 2250 6300
F 0 "X3" H 2100 6400 50  0000 L CNN
F 1 "Mac2" H 2300 6400 50  0000 L CNN
F 2 "SamacSys_Parts:mac_happy_small" H 2250 6300 50  0001 C CNN
F 3 "SamacSys_Parts:mac_happy_small" H 2250 6300 50  0001 C CNN
	1    2250 6300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR047
U 1 1 5EFE8C0E
P 2850 5400
F 0 "#PWR047" H 2850 5150 50  0001 C CNN
F 1 "GND" H 2855 5227 50  0000 C CNN
F 2 "" H 2850 5400 50  0001 C CNN
F 3 "" H 2850 5400 50  0001 C CNN
	1    2850 5400
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR046
U 1 1 5EFE8DBD
P 2450 5400
F 0 "#PWR046" H 2450 5150 50  0001 C CNN
F 1 "GND" H 2455 5227 50  0000 C CNN
F 2 "" H 2450 5400 50  0001 C CNN
F 3 "" H 2450 5400 50  0001 C CNN
	1    2450 5400
	1    0    0    -1  
$EndComp
Wire Notes Line
	2000 5900 3900 5900
Wire Notes Line
	3900 6450 2000 6450
Text Notes 2050 6400 0    50   ~ 0
Images
Text Notes 2100 5650 1    50   ~ 0
Mounting Holes
$Comp
L SamacSys_Parts:Logo X7
U 1 1 5F2D2B3B
P 2800 6150
F 0 "X7" H 2950 6200 50  0000 R CNN
F 1 "Pi" H 2750 6200 50  0000 R CNN
F 2 "SamacSys_Parts:pi_logo" H 2800 6150 50  0001 C CNN
F 3 "" H 2800 6150 50  0001 C CNN
	1    2800 6150
	-1   0    0    1   
$EndComp
$Comp
L SamacSys_Parts:Logo X4
U 1 1 5EFCDD94
P 2800 6050
F 0 "X4" H 2950 6100 50  0000 R CNN
F 1 "Raspberry Pi" H 2750 6100 50  0000 R CNN
F 2 "SamacSys_Parts:pi_logo" H 2800 6050 50  0001 C CNN
F 3 "" H 2800 6050 50  0001 C CNN
	1    2800 6050
	-1   0    0    1   
$EndComp
$Comp
L SamacSys_Parts:5748901-1 J8
U 1 1 5FA017A4
P 2900 2900
F 0 "J8" V 2719 2900 50  0000 C CNN
F 1 "CONFLY DB25" V 2810 2900 50  0000 C CNN
F 2 "SamacSys_Parts:57489011" H 4550 3200 50  0001 L CNN
F 3 "https://componentsearchengine.com/Datasheets/1/L717SDB25PA4CH4F.pdf" H 4550 3100 50  0001 L CNN
F 4 "D-Sub Standard Connectors 25P Size B Stamped Male DSub Contact SD" H 4550 3000 50  0001 L CNN "Description"
F 5 "12.55" H 4550 2900 50  0001 L CNN "Height"
F 6 "523-L717SDB25PA4CH4F" H 4550 2800 50  0001 L CNN "Mouser Part Number"
F 7 "https://www.mouser.com/Search/Refine.aspx?Keyword=523-L717SDB25PA4CH4F" H 4550 2700 50  0001 L CNN "Mouser Price/Stock"
F 8 "Amphenol" H 4550 2600 50  0001 L CNN "Manufacturer_Name"
F 9 "L717SDB25PA4CH4F" H 4550 2500 50  0001 L CNN "Manufacturer_Part_Number"
F 10 "C77838" V 2900 2900 50  0001 C CNN "LCSC"
	1    2900 2900
	0    1    1    0   
$EndComp
Text Notes 6750 7760 0    87   ~ 17
RaSCSI Daisy Chain Board
Text Notes 9900 7890 0    79   ~ 16
2.2
Text Notes 7450 7870 0    59   ~ 12
19-Aug-2020
Wire Notes Line
	2000 2700 5250 2700
Text GLabel 3550 1800 1    50   BiDi ~ 0
TERMPOW
Wire Notes Line
	5250 1300 5250 2700
Wire Notes Line
	2000 1300 2000 6450
$EndSCHEMATC
