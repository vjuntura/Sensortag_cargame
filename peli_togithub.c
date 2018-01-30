/*
 *  ======== main.c ========
 */
 //Väinö Juntura ja Arttu Pulkkinen

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>

/* XDCtools Header files */

#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Header files */
#include <ti/drivers/I2C.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include "tulokset"

/* Board Header files */
#include "Board.h"

/* jtkj Header files */
#include "wireless/comm_lib.h"
#include "sensors/bmp280.h"
#include "sensors/mpu9250.h"

//Tiedostojen tallennukseen headerit
#include <ctype.h>
#include <stddef.h>


/* Task Stacks */
#define STACKSIZE 4096
Char paaTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];

/* jtkj: Display */
Display_Handle hDisplay;

//Esitellään funktiot
Void menuFxn();
Void esteiden_selvittajaFxn();
Void tulosta_esteet();
Void liikuta_autoa();
Void tormaystesti();
Void nollaa_taulukko();
Void tallenna_highscore();
Void tulosta_highscore();
Void tulosta_kannustus();
Void peliFxn();
Void kalibroi();

// Napille "laskurit"

int valinta = 0;
int painettu = 0;
int laheta_viesti_nappi = 0;

//muuttuja törmäykselle
int tormays = 0;

//muuttuja missä score juoksee
int score = 0;

//Lista mihin scoret tallentuu
int highscore[10];

//Maskit esteiden selvittelyyn
uint8_t maski_liikkuva_este_oikea_reuna = 0x02;
uint8_t maski_liikkuva_este_oikea = 0x04;
uint8_t maski_kiintea_este_oikea = 0x08;
uint8_t maski_kiintea_este_vasen = 0x10;
uint8_t maski_liikkuva_este_vasen = 0x20;
uint8_t maski_liikkuva_este_vasen_reuna = 0x40;

//Muuttuja johon tallennetaan uusi pala kenttää puskurilta
uint8_t pala_kenttaa;

// viestipuskuri
char payload[16];

//Viestittelymuuttuja
char viesti[9];


//Taulukko johon esteet yms tallennetaan, ja josta kenttä tulostetaan
int taulukko[5][6] = {{0, 0, 0, 0, 0, 0},
                    {0, 0, 0, 0, 0, 0},
                    {0, 0, 0, 0, 0, 0},
                    {0, 0, 0, 0, 0, 0},
                    {0, 0, 0, 0, 0, 0}
                     };

//Sensori muuttujat
float ax, ay, az, gx, gy, gz;

//Globaalit muuttujat kalibroiduille arvoille
float vasen, oikea, hyppy;

//Auton paikkaa kuvaavat muutujat
int auto_vasen, auto_oikea, auto_ilma_vasen, auto_ilma_oikea;

//muttujat forlooppeihin
int rivi = 0;
int sarake = 0;


/* jtkj: Pin Button1 configured as power button */
static PIN_Handle hPowerButton;
static PIN_State sPowerButton;
PIN_Config cPowerButton[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};
PIN_Config cPowerWake[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE
};


/* jtkj: Pin Button0 configured as input */
static PIN_Handle hButton0;
static PIN_State sButton0;
PIN_Config cButton0[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

// alustus virtanapista valikkonappi
static PIN_Handle hButtonValikko;
static PIN_State bStateValikko;
PIN_Config buttonValikko[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

// alustus virtanapistaviestinappi
static PIN_Handle hButtonViesti;
static PIN_State bStateViesti;
PIN_Config buttonViesti[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};


//alustetaan mpu pin

static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};


// MPU9250 uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};


I2C_Handle i2cMPU;
I2C_Params i2cMPUParams;


/* jtkj: Handle power button */
Void powerButtonFxn(PIN_Handle handle, PIN_Id pinId) {

    Display_clear(hDisplay);
    Display_close(hDisplay);
    Task_sleep(100000 / Clock_tickPeriod);

	PIN_close(hPowerButton);
    PINCC26XX_setWakeup(cPowerWake);
	Power_shutdown(NULL,0);
}

// Käsittelijäfunktio button

Void Button0Fxn(PIN_Handle handle, PIN_Id pinId) {
    valinta = 1;
}

// Funktio jolla virtanappi muutetaan viestin lähettämiseen
Void Button1_Viesti(PIN_Handle handle, PIN_Id pinId) {
    laheta_viesti_nappi = 1;
}


// Funktio jolla virtanappi muutetaan valikossa liikkumiseen
Void Button1_Valikko(PIN_Handle handle, PIN_Id pinId) {
    painettu++;

    if(painettu == 4){
        painettu = 0;
    }
}


//Menufunktio

Void menuFxn(){
        valinta = 0;
        painettu = 0;

        // Määritetään virtanappi valikossa liikkumiseen
        PIN_close(hPowerButton);

        hButtonValikko = PIN_open(&bStateValikko, buttonValikko);
            if( !hButtonValikko) {
        System_abort("Error initializing button shut pins\n");
        }
        if (PIN_registerIntCb(hButtonValikko, &Button1_Valikko) != 0) {
            System_abort("Error registering button callback function");
        }

        Display_clear(hDisplay);

        //Looppi menulle
           while(1){
            switch (painettu) {
                case 0:
                    Display_print0(hDisplay, 3, 3, "X Peli");
                    Display_print0(hDisplay, 5, 3, "Score");
                    Display_print0(hDisplay, 7, 3, "Kalibroi");
                    Display_print0(hDisplay, 9, 3, "Sammuta");
                    if(valinta == 1){
                        valinta = 0;

                        //Katsotaan onko kalibroitu

                        if(vasen == 0 && oikea == 0 && hyppy == 0) {
                            Display_clear(hDisplay);
                            Display_print0(hDisplay, 3, 1, "Kalibroi ensin!");
                            Task_sleep(2000000/Clock_tickPeriod);
                            break;
                        }
                        //Jos on niin mennään peliin
                        else {
                            Task_sleep(100/Clock_tickPeriod);
                            //Muutetaan nappia
                            PIN_close(hButtonValikko);
  	                        hButtonViesti = PIN_open(&bStateViesti, buttonViesti);
	                            if(!hButtonViesti) {
		                            System_abort("Error initializing power button shut pins\n");
                            	    }
	                            if (PIN_registerIntCb(hButtonViesti, &Button1_Viesti) != 0) {
		                            System_abort("Error registering power button callback function");
	                            }
                            Display_clear(hDisplay);
                            peliFxn();

                            }
                    break;

                case 1:
                    Display_print0(hDisplay, 3, 3, "Peli");
                    Display_print0(hDisplay, 5, 3, "X Score");
                    Display_print0(hDisplay, 7, 3, "Kalibroi");
                    Display_print0(hDisplay, 9, 3, "Sammuta");
                    if(valinta == 1){
                        Task_sleep(1000/Clock_tickPeriod);
                        valinta = 0;
                        Display_clear(hDisplay);
                        tulosta_highscore();
                    }
                    break;
                case 2:
                    Display_print0(hDisplay, 3, 3, "Peli");
                    Display_print0(hDisplay, 5, 3, "Score");
                    Display_print0(hDisplay, 7, 3, "X Kalibroi");
                    Display_print0(hDisplay, 9, 3, "Sammuta");
                    if(valinta == 1){
	                    Task_sleep(100000/Clock_tickPeriod);
                        valinta = 0;
                        PIN_close(hButtonValikko);
                        //Tässä  kalibroimaa
                        kalibroi();
                            }


                    break;
                case 3:
                    Display_print0(hDisplay, 3, 3, "Peli");
                    Display_print0(hDisplay, 5, 3, "Score");
                    Display_print0(hDisplay, 7, 3, "Kalibroi");
                    Display_print0(hDisplay, 9, 3, "X Sammuta");
                    if(valinta == 1){
                        Task_sleep(1000/Clock_tickPeriod);
                        valinta = 0;
                        //Tässä muutetaan button 0 takas sammutusnapiksi
                        Display_clear(hDisplay);
                        Display_print0(hDisplay, 3, 3, "Sammuta");
                        PIN_close(hButtonValikko);
  	                    hPowerButton = PIN_open(&sPowerButton, cPowerButton);
	                        if(!hPowerButton) {
		                        System_abort("Error initializing power button shut pins\n");
                            	}
	                        if (PIN_registerIntCb(hPowerButton, &powerButtonFxn) != 0) {
		                        System_abort("Error registering power button callback function");
	                            }
	                    Task_sleep(2000000/Clock_tickPeriod);



                        // Määritetään virtanappi takas valikossa liikkumiseen ennenkuin palataan takas valikkoon
                        PIN_close(hPowerButton);
                        Display_clear(hDisplay);

                        hButtonValikko = PIN_open(&bStateValikko, buttonValikko);
                        if( !hButtonValikko) {
                            System_abort("Error initializing button shut pins\n");
                        }
                        if (PIN_registerIntCb(hButtonValikko, &Button1_Valikko) != 0) {
                            System_abort("Error registering button callback function");
                        }
	                    Task_sleep(1000/Clock_tickPeriod);
                    break;
                    }

            }

            Task_sleep(100000/Clock_tickPeriod);
           }

        }
}


//Maskaa esteet bittijonosta ja muuttelee taulukkoa niiden mukaan
Void esteiden_selvittajaFxn(){


    //Siirretään pelikenttää pykälä alaspäin
    //siirrellään merkkejä taulukossa
    for(rivi = 4; rivi > 0; rivi--){
        for(sarake = 0; sarake < 6; sarake++){
            //nollataan ruutu ensin
            taulukko[rivi][sarake] = 0;
            //paikallaan pysyvä este
            if(taulukko[(rivi - 1)][sarake] == 1){
                taulukko[rivi][sarake] = 1;
            }
            //vasemmalta reunasta lähtevä
            if((taulukko[(rivi - 1)][(sarake - 2)] == 2) && ((rivi == 2) || (rivi == 3))){
                taulukko[rivi][sarake] = 2;
            }
            if((taulukko[(rivi - 1)][(sarake - 1)] == 2) && (rivi == 1)){
                taulukko[rivi][sarake] = 2;
            }
            if((taulukko[(rivi - 1)][(sarake + 2)] == 2) && (rivi == 4)){
                taulukko[rivi][sarake] = 2;
            }
            //oikealta reunasta lähtevä
            if((taulukko[(rivi - 1)][(sarake + 2)] == 5) && ((rivi == 2) || (rivi == 3))){
                taulukko[rivi][sarake] = 5;
            }
            if((taulukko[(rivi - 1)][(sarake + 1)] == 5) && (rivi == 1)){
                taulukko[rivi][sarake] = 5;
            }
            if((taulukko[(rivi - 1)][(sarake - 2)] == 5) && (rivi == 4)){
                taulukko[rivi][sarake] = 5;
            }

            //oikealta vasemmalle liikkuva
            if((taulukko[(rivi - 1)][(sarake - 2)] == 4) && (rivi >= 3) && (sarake > 1)){
                taulukko[rivi][sarake] = 4;
            }
            if((taulukko[(rivi - 1)][(sarake + 2)] == 4) && (rivi < 3) && (sarake < 3)){
                taulukko[rivi][sarake] = 4;
            }
            //vasemalta oikealle liikkuva
            if((taulukko[(rivi - 1)][(sarake - 2)] == 3) && (rivi < 3) && (sarake > 2)){
                taulukko[rivi][sarake] = 3;
            }
            if((taulukko[(rivi - 1)][(sarake + 2)] == 3) && (rivi >= 3) && (sarake < 4)){
                taulukko[rivi][sarake] = 3;
            }

        }

    }
    //nollataan ylärivi
    for(sarake = 0; sarake < 6; sarake++){
        taulukko[0][sarake] = 0;
    }
    //Selvitellään onko esteitä ja merkitään taulukkoon missä on
    if((pala_kenttaa & maski_liikkuva_este_oikea_reuna) == 2){
        taulukko[0][5] = 5;
    }
    if((pala_kenttaa & maski_liikkuva_este_oikea) == 4){
        taulukko[0][4] = 4;
    }
    if((pala_kenttaa & maski_kiintea_este_oikea) == 8){
        taulukko[0][3] = 1;
    }
    if((pala_kenttaa & maski_kiintea_este_vasen) == 16){
        taulukko[0][2] = 1;
    }
    if((pala_kenttaa & maski_liikkuva_este_vasen) == 32){
        taulukko[0][1] = 3;
    }
    if((pala_kenttaa & maski_liikkuva_este_vasen_reuna) == 64){
        taulukko[0][0] = 2;
    }
    //Nollataan kenttä
    pala_kenttaa = 0;
}

//Tulostaa esteet

Void tulosta_esteet(){
    //Täytetään kaikki rivit tyhjällä
    for(rivi = 2; rivi < 11; rivi++){
        Display_print0(hDisplay, (rivi), 0, "  |    |    |   ");
    }
    for(rivi = 0; rivi < 5; rivi++){
        //76 = 'L' ja 88 = X
        char esteet[17] = "  |    |    |   ";
       //Liikuva oikea reuna
        if(taulukko[rivi][0] > 0){
            esteet[0] = 'L';
            esteet[1] = 76;
        }
        //Liikuva vasen kaista
        if((taulukko[rivi][1] > 0) || (taulukko[rivi][2] > 1)){
            esteet[3] = 76;
            esteet[4] = 76;
            esteet[5] = 76;
            esteet[6] = 76;
        }
        //Paikallaan oleva vasen
        if(taulukko[rivi][2] == 1){
            esteet[3] = 88;
            esteet[4] = 88;
            esteet[5] = 88;
            esteet[6] = 88;
        }
        //Paikallaan oleva oikea
        if(taulukko[rivi][3] == 1){
            esteet[8] = 88;
            esteet[9] = 88;
            esteet[10] = 88;
            esteet[11] = 88;
        }
        //Liikuva oikea kaista
        if((taulukko[rivi][4] > 0) || (taulukko[rivi][3] > 1)){
            esteet[8] = 76;
            esteet[9] = 76;
            esteet[10] = 76;
            esteet[11] = 76;
        }
        if(taulukko[rivi][5] > 0){
            esteet[13] = 76;
            esteet[14] = 76;
        }
        if(rivi == 4){
        Display_print0(hDisplay, ((rivi + 1) * 2), 0, esteet);
        }
        else{
        Display_print0(hDisplay, ((rivi + 1) * 2), 0, esteet);
        Display_print0(hDisplay, ((rivi + 1) * 2 + 1), 0, esteet);
        }
    }


}

//Liikuttaa autoa

Void liikuta_autoa(){

    // kysellään dataa mpulta
    //    Accelerometer values: ax,ay,az
	 //    Gyroscope values: gx,gy,gz
	mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

    //siirrellään autoa

    //Jos auto ollu ilmassa vasemmalla, niin siirretään takas radalle
    if(auto_ilma_vasen == 1){
        Display_print0(hDisplay, 11, 0, "  |AUTO|    |   ");
        auto_vasen = 1;
        auto_oikea = 0;
        auto_ilma_vasen = 0;
        auto_ilma_oikea = 0;
    }

    //Jos auto ollu ilmassa oikealla, niin siirretään takas radalle
    else if(auto_ilma_oikea == 1){
        Display_print0(hDisplay, 11, 0, "  |    |AUTO|   ");
        auto_vasen = 0;
        auto_oikea = 1;
        auto_ilma_vasen = 0;
        auto_ilma_oikea = 0;
    }
    //Auto oikealle
    else if(ax >= oikea){
        Display_print0(hDisplay, 11, 0, "  |    |AUTO|   ");
        auto_vasen = 0;
        auto_oikea = 1;
        auto_ilma_vasen = 0;
        auto_ilma_oikea = 0;
    }

    //Auto vasemmalle
    else if(ax <= vasen){
        Display_print0(hDisplay, 11, 0, "  |AUTO|    |   ");
        auto_vasen = 1;
        auto_oikea = 0;
        auto_ilma_vasen = 0;
        auto_ilma_oikea = 0;
    }

    //hypätään oikealta
    else if(az <= hyppy && auto_oikea == 1){
        Display_print0(hDisplay, 11, 0, "  |    |    |AU ");
        auto_vasen = 0;
        auto_oikea = 0;
        auto_ilma_vasen = 0;
        auto_ilma_oikea = 1;
    }

    //hypätään vasemmalta
    else if(az <= hyppy && auto_vasen == 1){
        Display_print0(hDisplay, 11, 0, "AU|    |    |   ");
        auto_vasen = 0;
        auto_oikea = 0;
        auto_ilma_oikea = 0;
        auto_ilma_vasen = 1;
    }
}

//Törmäystesti
Void tormaystesti(){

    //Törmäys oikealla kaistalla
    if(auto_oikea == 1 && auto_ilma_vasen == 0 && auto_ilma_oikea == 0 && (taulukko[4][4] >= 1 || taulukko[4][3] >= 1)) {
        tormays = 1;
        Task_sleep(100000 / Clock_tickPeriod);
    }
    //Törmäys vasemmalla
    else if(auto_vasen == 1 && auto_ilma_vasen == 0 && auto_ilma_oikea == 0 && (taulukko[4][2] >= 1 || taulukko[4][1] >= 1)) {
        tormays = 1;
        Task_sleep(100000 / Clock_tickPeriod);

    }
    //ei törmäystä
    else{
        tormays = 0;
    }
}

//Nollaa taulukon pelin loppuessa
Void nollaa_taulukko(){


    for(rivi = 0; rivi < 5; rivi++){
        for(sarake = 0; sarake < 6; sarake++){
            taulukko[rivi][sarake] = 0;
    }
    }

}

//Funktio scorejen tallennukseen

Void tallenna_highscore(){
    for(rivi = 10; rivi > 0; rivi--){
        highscore[rivi] = highscore [(rivi - 1)];
    }
    highscore[0] = score;
}

//Funktio scorejen tulostukseen

Void tulosta_highscore() {
    char arvo[4];
    Display_print0(hDisplay, 1, 0, " Huippupisteet:");
    for(rivi = 0; rivi < 10; rivi++){
        sprintf(arvo, "%d", highscore[rivi]);
        Display_print0(hDisplay, (rivi +2), 7, arvo);
    }

    Task_sleep(5000000 / Clock_tickPeriod);
    valinta = 0;
    Display_clear(hDisplay);
}

//Tulostaa kannustusviestin

Void tulosta_kannustus(){


    if((score % 30) == 0){
        Display_print0(hDisplay, 1, 1, "Loistavaa!");
    }
    else if((score % 20) == 0){
        Display_print0(hDisplay, 1, 1, "Upeaa!");
    }
    else if((score % 10) == 0){
        Display_print0(hDisplay, 1, 1, "Mahtavaa!");
    }
    else{
        Display_print0(hDisplay, 1, 2, viesti);
        memset(viesti, 0, 9);
    }

}


//Itse pelin toteuttava funktio

Void peliFxn() {
    //Nollaillaan ja luodaan muuttujia
    tormays = 0;
    score = 0;
    char pisteet[5];
    nollaa_taulukko();
    Display_clear(hDisplay);
    //Määritellään lähetettävä viesti
    char viesti_serverille[16];
    sprintf(viesti_serverille, "Olen %x", IEEE80154_MY_ADDR);


    // tulostetaan lähtölaskenta
    Display_print0(hDisplay, 5, 1, "Peli alkaa:");
    Display_print0(hDisplay, 6, 7, "3");
    Task_sleep(1000000 / Clock_tickPeriod);
    Display_print0(hDisplay, 5, 1, "Peli alkaa:");
    Display_print0(hDisplay, 6, 7, "2");
    Task_sleep(1000000 / Clock_tickPeriod);
    Display_print0(hDisplay, 5, 1, "Peli alkaa:");
    Display_print0(hDisplay, 6, 7, "1");
    valinta = 0;
    Task_sleep(1000000 / Clock_tickPeriod);
    //Täytetään kaikki rivit tyhjällä
    for(rivi = 2; rivi < 11; rivi++){
        Display_print0(hDisplay, (rivi), 0, "  |    |    |   ");
    }
    //Alustetaan auto vasemmalle
    Display_print0(hDisplay, 11, 0, "  |AUTO|    |   ");
    auto_vasen = 1;

    while(1){


        //Lopetetaan peli napin painalluksesta
        if(valinta == 1){
            valinta = 0;
            Display_clear(hDisplay);
            Display_print0(hDisplay, 5, 1, "Lopetit pelin!");
            Task_sleep(3000000 / Clock_tickPeriod);
            break;
        }
        //pelataan jos nappia ei paineta
        else {

            score = score + 1;
            sprintf(pisteet, "Pisteet: %d", score);
            Display_print0(hDisplay, 0, 1, pisteet);
            tulosta_kannustus();
            esteiden_selvittajaFxn();
            tulosta_esteet();

            //Katotaan onko törmätty
            tormaystesti();
            if(tormays == 1){
                tallenna_highscore();
                Display_clear(hDisplay);
                Display_print0(hDisplay, 5, 1, "Tappio!");
                sprintf(pisteet, "Pisteesi: %d", score);
                Display_print0(hDisplay, 6, 1, pisteet);
                Task_sleep(2000000 / Clock_tickPeriod);
                break;

            }
            //Lähetetään viesti napin painalluksesta
            if(laheta_viesti_nappi == 1){
                laheta_viesti_nappi = 0;
                Send6LoWPAN(IEEE80154_SERVER_ADDR, viesti_serverille, 16);
                StartReceive6LoWPAN();

            }
            Task_sleep(900000 / Clock_tickPeriod);
            liikuta_autoa();
        }



    }

    Display_clear(hDisplay);
    valinta = 0;
    PIN_close(hButtonViesti);
    Task_sleep(1000 / Clock_tickPeriod);
    menuFxn();

}


//funktio laitteen kalibrointiin

Void kalibroi() {
    //nollataan kalibrointiarvot
    vasen = 0;
    oikea = 0;
    hyppy = 0;
    valinta = 0;
    Display_clear(hDisplay);


    //Kalibroidaan vasemmalle
    while(1){
	    // kysellään dataa mpulta
        //    Accelerometer values: ax,ay,az
	 	//    Gyroscope values: gx,gy,gz
		mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
        if(valinta == 0){
            Display_print0(hDisplay, 1, 14, "^");
            Display_print0(hDisplay, 2, 14, "|");
            Display_print0(hDisplay, 3, 14, "|");
            Display_print0(hDisplay, 4, 14, "|");
            Display_print0(hDisplay, 5, 1, "Kallista vasem-");
            Display_print0(hDisplay, 6, 1, "malle ja paina");
            Display_print0(hDisplay, 7, 5, "nappia");
        }
        else if(valinta == 1) {
            vasen = 0.9 * ax;
            valinta = 0;
            Task_sleep(1000/Clock_tickPeriod);
            break;
        }
        Task_sleep(1000000/Clock_tickPeriod);
    }
    Display_clear(hDisplay);
    Display_print0(hDisplay, 5, 1, "Kiitos!");
    Task_sleep(1000000/Clock_tickPeriod);
    valinta = 0;

    //Kalimbroidaan oikealle
    while(1){
		mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
        if(valinta == 0){
            Display_print0(hDisplay, 1, 14, "^");
            Display_print0(hDisplay, 2, 14, "|");
            Display_print0(hDisplay, 3, 14, "|");
            Display_print0(hDisplay, 4, 14, "|");
            Display_print0(hDisplay, 5, 1, "Kallista oikea-");
            Display_print0(hDisplay, 6, 1, "lle ja paina");
            Display_print0(hDisplay, 7, 5, "nappia");
        }
        else if(valinta == 1) {
            oikea = 0.9 * ax;
            valinta = 0;
            Task_sleep(1000/Clock_tickPeriod);
            break;
        }
        Task_sleep(1000000/Clock_tickPeriod);
    }
    Display_clear(hDisplay);
    Display_print0(hDisplay, 5, 1, "Kiitos!");
    Task_sleep(1000000/Clock_tickPeriod);
    valinta = 0;

    //Kalibroidaan hyppy
    while(1){
		mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
        if(valinta == 0){
            Display_print0(hDisplay, 1, 14, "^");
            Display_print0(hDisplay, 2, 14, "|");
            Display_print0(hDisplay, 3, 14, "|");
            Display_print0(hDisplay, 4, 14, "|");
            Display_print0(hDisplay, 5, 1, "Kallista eteen");
            Display_print0(hDisplay, 6, 3, "ja paina");
            Display_print0(hDisplay, 7, 5, "nappia");
        }
        else if(valinta == 1) {
            hyppy = 0.9 * az;
            valinta = 0;
            Task_sleep(1000/Clock_tickPeriod);
            break;
        }
        Task_sleep(1000000/Clock_tickPeriod);
    }
    Display_clear(hDisplay);
    Display_print0(hDisplay, 5, 1, "Kiitos!");
    Display_print0(hDisplay, 6, 1, "Kalibrointi OK!");
    Task_sleep(2000000/Clock_tickPeriod);
    Display_clear(hDisplay);
    valinta = 0;
    Task_sleep(1000 / Clock_tickPeriod);
    menuFxn();
}



Void paaTask() {

    /* jtkj: Init Display */
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (hDisplay == NULL) {
        System_abort("Error initializing Display\n");
    }
    Display_print0(hDisplay, 5, 2, "Odota hetki");

// MPU hommat
    //Määritellään i2c


    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // Avataan iac mpulle

    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU käyntiin

    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    //Odotetaan 100ms että mpu käynnistyy
	Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU asetukset ja kaliroitni

	System_printf("MPU9250: Setup and calibration...\n");
	System_flush();

	mpu9250_setup(&i2cMPU);

	System_printf("MPU9250: Setup and calibration OK\n");
	System_flush();



    menuFxn();


}

// Tiedonsiirtotaski
Void commTask(UArg arg0, UArg arg1) {

    uint16_t senderAddr;

   // Radio alustetaan vastaanottotilaan
   int32_t result = StartReceive6LoWPAN();
   if(result != true) {
      System_abort("Wireless receive start failed");
   }


    // Lähetyksen jälkeen taas vastaanottotilaan
    StartReceive6LoWPAN();

   // Vastaanotetaan viestejä loopissa
    while (1) {
        // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
        memset(payload,0,16);

        // jos true, viesti odottaa
        if (GetRXFlag() == true) {
            // Luetaan viesti puskuriin payload
            Receive6LoWPAN(&senderAddr, payload, 16);
            //siirretään kenttää vastaava merkki toiseen muuttujaan
            pala_kenttaa = payload[0];
            //siirretään lopunt viestistä toiseen muuttujaan
            sprintf(viesti, "%c%c%c%c%c%c%c%c", payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7], payload[8]);

      }
}
}



Int main(void) {


    // Task variables
	Task_Handle hPaaTask;
	Task_Params paaTaskParams;
	Task_Handle hCommTask;
	Task_Params commTaskParams;



    // Initialize board
    Board_initGeneral();
    Board_initI2C();



	/* jtkj: Power Button */
	hPowerButton = PIN_open(&sPowerButton, cPowerButton);
	if(!hPowerButton) {
		System_abort("Error initializing power button shut pins\n");
	}
	if (PIN_registerIntCb(hPowerButton, &powerButtonFxn) != 0) {
		System_abort("Error registering power button callback function");
	}

    // JTKJ: INITIALIZE BUTTON0 HERE
      hButton0 = PIN_open(&sButton0, cButton0);
   if(!hButton0) {
      System_abort("Error initializing button pins\n");
     }

   if (PIN_registerIntCb(hButton0, &Button0Fxn) != 0) {
      System_abort("Error registering button callback function");
    }


    /* jtkj: Init Main Task */
    Task_Params_init(&paaTaskParams);
    paaTaskParams.stackSize = STACKSIZE;
    paaTaskParams.stack = &paaTaskStack;
    paaTaskParams.priority=2;

    hPaaTask = Task_create(paaTask, &paaTaskParams, NULL);
    if (hPaaTask == NULL) {
    	System_abort("Task create failed!");
    }


  /* jtkj: Init Communication Task */
    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=1;

    Init6LoWPAN();

    hCommTask = Task_create(commTask, &commTaskParams, NULL);
    if (hCommTask == NULL) {
    	System_abort("Task create failed!");
    }

    /* Start BIOS */
    BIOS_start();

    return (0);
}
