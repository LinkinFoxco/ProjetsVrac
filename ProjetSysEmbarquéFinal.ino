#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <ChainableLED.h>
#include <Wire.h>
#include "DS1307.h"


#define LIGHTPIN A0                                 //define des entree sorties
#define PRESSUREPIN 6
#define HUMIDPIN 4
#define TEMPERATUREPIN 5
#define BUTTONRED 3
#define BUTTONGREEN 2
#define PINSD 4
#define LED1 7
#define LED2 8
#define TEST_N_VALUE random(1,10)
#define TEST_E_VALUE random(1,10)
#define TEST_M_VALUE random(1,10)
#define LUX random(256,727)
#define PRESSURE random(851,1079)
#define HUMID random(1,49)
#define TEMPERATURE random(1,59)

/* ------------------------------------------------------------------------------------ */

DS1307 clock;                                       // initialise la clock

volatile bool hasInterruptR = false;                 // variables pour les interupts
volatile bool hasInterruptV = false;
volatile long temp = 0;
volatile long temp2 = 0;
volatile long verif = 0;

ChainableLED leds(LED1, LED2, 1);                           // fixe les pins de la led

/* ------------------------------------------------------------------------------------ */

File dataFile;                                  //pour la creation du fichier dataFile qui contient les donn�es sur la carte SD
String filename = "DONNEES.txt";                    //le nom du fichier texte qui est stock� sur la SD

/* ------------------------------------------------------------------------------------ */

struct GPS 
{

    float TestN;
    float TestE;
    float TestM;

};                             // structure du GPS 

enum JourDeLaSemaine 
{

    LUNDI,
    MARDI,
    MERCREDI,
    JEUDI,
    VENDREDI,
    SAMEDI,
    DIMANCHE

};       // enum des jours de la semaine

struct TempsStruct 
{ // Heure,Minute,Seconde,Mois,Annee,Jour,JDLS

    unsigned short Heure;
    unsigned short Minute;
    unsigned short Seconde;
    unsigned short Mois;
    unsigned short Annee;
    unsigned short Jour;
    JourDeLaSemaine JDLS;

};             // structure du temps

struct Mesure 
{

    float Temperature;
    unsigned short Lux;
    unsigned short Hygr;
    unsigned short Pressure;
    GPS Position;
    TempsStruct Temps;

};                       // structure des mesures

struct Parametre
{

    unsigned long Log_Intervall;
    unsigned long File_Max_Size;
    unsigned long Timeout;
    unsigned short Lumin_Low;
    unsigned short Lumin_High;
    float Min_Temp_Air;
    float Max_Temp_Air;
    unsigned short Hygr_MinT;
    unsigned short Hygr_MaxT;
    unsigned short Pressure_Min;
    unsigned short Pressure_Max;
    bool Pressure;
    bool Hygr;
    bool Temp_Air;
    bool Lumin;
    TempsStruct Temps;

};                 // structure des parametres ( Intervalle , file max size , timeout, lux, tempe , hygr, pression, temps )

enum typeVariable {

    UNSHORT,
    UNLONG,
    FLOAT,
    BOOL,
    JDLS
};

struct ConfigLines {

    typeVariable type;
    void* adresse;

};

enum ModeSys {

    CONFIGURATION,
    STANDARD,
    MAINTENANCE,
    ECONOMIQUE

};                       // enum des differents modes

/* ------------------------------------------------------------------------------------ */

// boucle pour les erreurs

void boucleInfiniErreur(unsigned char Couleur1R, unsigned char Couleur1G, unsigned char Couleur1B, unsigned char Couleur2R, unsigned char Couleur2G, unsigned char Couleur2B, unsigned short TempsMiddle)
{

    long previousMillis = millis();
    while (true)
    {
        long currentMillis = millis();
        if ((currentMillis - previousMillis) < TempsMiddle)
            allumerLEDS(Couleur1R, Couleur1G, Couleur1B);
        else
        {
            if ((currentMillis - previousMillis) > TempsMiddle && (currentMillis - previousMillis) < 1000)
                allumerLEDS(Couleur2R, Couleur2G, Couleur2B);
            else
                previousMillis = millis();
        }
        delay(100);
    }

}

/* ------------------------------------------------------------------------------------ */

void obtenirGPS(GPS* positionMesure, bool* diviser, ModeSys* modeActuel)
{

    if (*modeActuel != ECONOMIQUE || *diviser == true)
    {
        positionMesure->TestE = TEST_E_VALUE;
        positionMesure->TestN = TEST_N_VALUE;
        positionMesure->TestM = TEST_M_VALUE;

        if (0 /* FLUX == NULL, on simule le capteur */)
        {
            Serial.println(F("ERREUR_GPS"));
            boucleInfiniErreur(255, 0, 0, 255, 128, 0, 500);
        }
        *diviser = false;
    }
    else
        *diviser = true;

}       // permet d aquerir les données GPS 

void obtenirTemps(TempsStruct* tempsMesure, int* TimeChecker2)
{
    clock.getTime();
    int temporaire = clock.minute * 60 + clock.second;
    if (*TimeChecker2 == temporaire)
    {
        Serial.println(F("ERREUR_RTC"));
        boucleInfiniErreur(255, 0, 0, 0, 0, 255, 500);
    }
    else
    {
        switch (clock.dayOfWeek) {
        case MON:
            tempsMesure->JDLS = LUNDI;
            break;
        case TUE:
            tempsMesure->JDLS = MARDI;
            break;
        case WED:
            tempsMesure->JDLS = MERCREDI;
            break;
        case THU:
            tempsMesure->JDLS = JEUDI;
            break;
        case FRI:
            tempsMesure->JDLS = VENDREDI;
            break;
        case SAT:
            tempsMesure->JDLS = SAMEDI;
            break;
        case SUN:
            tempsMesure->JDLS = DIMANCHE;
            break;
        }
        tempsMesure->Annee = clock.year + 2000;
        tempsMesure->Jour = clock.dayOfMonth;
        tempsMesure->Mois = clock.month;
        tempsMesure->Heure = clock.hour;
        tempsMesure->Minute = clock.minute;
        tempsMesure->Seconde = clock.second;
    }

}       // permet d aquerir les donnees temporels

void obtenirLuminosite(unsigned short* luxMesure, Parametre* Actuel)
{

    short lux = analogRead(LIGHTPIN);
    //    short lux = LUX;
    if (lux > Actuel->Lumin_High || lux < Actuel->Lumin_Low)
    {
        Serial.println(F("ERREUR_DATA"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 666);
    }
    else
        *luxMesure = lux;

    if (lux == false)
    {
        Serial.println(F("ERREUR_CPTR_LUMIN"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 500);
    }

}       // permet d aquerir la luminosite

void obtenirPression(unsigned short* pressionMesure, Parametre* Actuel)
{

    //short pression = analogRead(PRESSUREPIN);
    short pression = PRESSURE;
    if (pression > Actuel->Pressure_Max || pression < Actuel->Pressure_Min)
    {
        Serial.println(F("ERREUR_DATA"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 666);
    }
    else
        *pressionMesure = pression;

    if (pression == false)
    {
        Serial.println(F("ERREUR_CPTR_PRESS"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 500);
    }

}       // permet d aquerir la pression

void obtenirHumidite(unsigned short* hygrMesure, Parametre* Actuel)
{

    //short hygr = analogRead(HUMIDPIN);
    short hygr = HUMID;
    if (hygr > Actuel->Hygr_MaxT || hygr < Actuel->Hygr_MinT)
    {
        Serial.println(F("ERREUR_DATA"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 666);
    }
    else
        *hygrMesure = hygr;

    if (hygr == false)
    {
        Serial.println(F("ERREUR_CPTR_HYGRO"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 500);
    }

}       // permet d aquerir l'humidite

void obtenirTemperature(float* temperatureMesure, Parametre* Actuel)
{

    //short temperature = analogRead(TEMPERATUREPIN);
    short temperature = TEMPERATURE;
    if (temperature > Actuel->Max_Temp_Air || temperature < Actuel->Min_Temp_Air)
    {
        Serial.println(F("ERREUR_DATA"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 666);
    }
    else
        *temperatureMesure = temperature;

    if (temperature == false)
    {
        Serial.println(temperature);
        Serial.println(F("ERREUR_CPTR_TEMPE"));
        boucleInfiniErreur(255, 0, 0, 0, 255, 0, 500);
    }

}       // permet d aquerir la temperature

void retirerSD(bool* stocker)
{

if (SD.exists(filename) == true)
    {
        dataFile = SD.open(filename);
        dataFile.flush();
        dataFile.close();
        Serial.println(F("Vous pouvez retirer la carte SD"));
    }
    else if (SD.exists(filename) == false)
    {
        Serial.println(F("Erreur: Le fichier est inexistant."));
    }
  *stocker = false;

}       // retire la carte SD

void ajouterSD(bool* stocker)
{

// configurer les variables � l'aide des fonctions de la biblioth�que de l'utilitaire SD

    Sd2Card card;
    SdVolume volume;
    SdFile root;

    //Initialisation de la carte SD

    Serial.print(F("\n Initialisation de la carte SD en cours ... "));
    if (!SD.begin(PINSD))
    {
        *stocker = false;
        Serial.println(F("Fail"));
    }
    else
    {
        *stocker = true;
        Serial.println(F("Success"));
    }

//    // Definition du type de carte SD 
//
//    Serial.print("\nType de carte SD : ");
//    switch (card.type()) {
//    case SD_CARD_TYPE_SD1:
//        Serial.println("SD1");
//        break;
//    case SD_CARD_TYPE_SD2:
//        Serial.println("SD2");
//        break;
//    case SD_CARD_TYPE_SDHC:
//        Serial.println("SDHC");
//        break;
//    default:
//        Serial.println("Inconnu");
//    }

//    if (!volume.init(card))
//    {
//        Serial.println("Impossible de trouver la partition FAT16 / FAT32. \n Verifiez que vous avez formate la carte");
//    }

//    // afficher le type et la taille 
//
//    uint32_t volumesize;
//    Serial.print("\nLe type de Volume est FAT");
//    Serial.println(volume.fatType(), DEC);
//    Serial.println();


}       //ajoute la carte SD

void ValeurDefaut(Parametre *Defaut)
{

    Defaut->Log_Intervall = 8800;        //600000-1200
    Defaut->File_Max_Size = 4000000000;
    Defaut->Timeout = 30000;
    Defaut->Lumin_Low = 0;
    Defaut->Lumin_High = 768;
    Defaut->Min_Temp_Air = -10;
    Defaut->Max_Temp_Air = 60;
    Defaut->Hygr_MinT = 0;
    Defaut->Hygr_MaxT = 50;
    Defaut->Pressure_Min = 850;
    Defaut->Pressure_Max = 1080;
    Defaut->Lumin = true;
    Defaut->Temp_Air = false; //à changer
    Defaut->Hygr = true;
    Defaut->Pressure = true;
    Defaut->Temps.Annee = 2020;
    Defaut->Temps.Mois = 10;
    Defaut->Temps.Jour = 22;
    Defaut->Temps.Heure = 17;
    Defaut->Temps.Minute = 42;
    Defaut->Temps.Seconde = 30;
    Defaut->Temps.JDLS = JEUDI;

}

/* ------------------------------------------------------------------------------------ */

// fonctions d interruption

void boutonRougeAppuye()
{

    temp = millis();
    detachInterrupt(digitalPinToInterrupt(BUTTONRED));
    attachInterrupt(digitalPinToInterrupt(BUTTONRED), boutonRougeRelache, RISING);

}

void boutonRougeRelache()
{

    temp2 = millis();
    hasInterruptR = true;
    detachInterrupt(digitalPinToInterrupt(BUTTONRED));

}

void boutonVertAppuye()
{
    temp = millis();
    detachInterrupt(digitalPinToInterrupt(BUTTONGREEN));
    attachInterrupt(digitalPinToInterrupt(BUTTONGREEN), boutonVertRelache, RISING);

}

void boutonVertRelache()
{

    temp2 = millis();
    hasInterruptV = true;
    detachInterrupt(digitalPinToInterrupt(BUTTONGREEN));

}

/* ------------------------------------------------------------------------------------ */
//
void configuration(ModeSys* modeAncien, ModeSys* modeActuel, Parametre* Actuel)
{
//    allumerLEDS(255, 255, 0);
//
//    Serial.println(F("ModeSys Configuration\nVeuillez ecrire les valeurs modifiées\nAu bout de 30 minutes le programme retournera en mode standard\n"));
//    delay(10000);
//
//    long duree = 0;
//    long previousMillis = 0;
//    long currentMillis = 0;
//    bool verif = false;
//    int inBytes = 0;
//    int lastInBytes = 0;
//
//    String tableauQuestions[22] = { 
//        F("Log-Intervall (en seconde)\n"),
//        F("File_Max_Size (en octets)\n"),
//        F("Timeout (en seconde)\n"),
//        F("Lumin (true or false)\n"),
//        F("Lumin_Low (par défaut : 255)\n"),
//        F("Lumin_High(par défaut : 768)\n"),
//        F("Temp_air (true or false)\n"),
//        F("Min_Temp_Air (par défaut : -10)\n"),
//        F("Max_Temp_Air(par défaut : 60)\n"),
//        F("Hygr (true or false)\n"),
//        F("Hygr_MinT (par défaut : 0)\n"),
//        F("Hygr_MaxT (par défaut : 50)\n"),
//        F("Pressure (true or false)\n"),
//        F("Pressure_Min (par défaut : 850)\n"),
//        F("Pressure_Max (par défaut : 1080)\n"),
//        F("Temps.Heure\n"),
//        F("Temps.Minute\n"),
//        F("Temps.Seconde\n"),
//        F("Temps.Mois\n"),
//        F("Temps.Jour\n"),
//        F("Temps.Annee\n"),
//        F("Temps.JDLS (ex : MON)\n"),
//    };
//
//    ConfigLines tableauConfig[22] = {
//        {UNLONG, &(Actuel->Log_Intervall)},
//        {UNLONG, &(Actuel->File_Max_Size)},
//        {UNLONG, &(Actuel->Timeout)},
//        {BOOL, &(Actuel->Lumin)},
//        {UNSHORT, &(Actuel->Lumin_Low)},
//        {UNSHORT, &(Actuel->Lumin_High)},
//        {BOOL, &(Actuel->Temp_Air)},
//        {UNSHORT, &(Actuel->Min_Temp_Air)},
//        {UNSHORT, &(Actuel->Max_Temp_Air)},
//        {BOOL, &(Actuel->Hygr)},
//        {FLOAT, &(Actuel->Hygr_MinT)},
//        {FLOAT, &(Actuel->Hygr_MaxT)},
//        {BOOL, &(Actuel->Pressure)},
//        {UNSHORT, &(Actuel->Pressure_Min)},
//        {UNSHORT, &(Actuel->Pressure_Max)},
//        {UNSHORT, &(Actuel->Temps.Heure)},
//        {UNSHORT, &(Actuel->Temps.Minute)},
//        {UNSHORT, &(Actuel->Temps.Seconde)},
//        {UNSHORT, &(Actuel->Temps.Mois)},
//        {UNSHORT, &(Actuel->Temps.Jour)},
//        {UNSHORT, &(Actuel->Temps.Annee)},
//        {JDLS, &(Actuel->Temps.JDLS)},
//    };
//
//    for (int i = 0; i < 22; i++)
//    {
//        Serial.println(tableauQuestions[i]);
//        previousMillis = millis();
//        while (verif == false && duree < 180000) {
//            inBytes = Serial.available();
//            if (inBytes != lastInBytes && inBytes > 0)
//            {
//                switch (tableauConfig[i].type)
//                {
//                case UNLONG:
//                    *((unsigned long*)tableauConfig[i].adresse) = Serial.read();
//                    break;
//                case UNSHORT:
//                    *((unsigned short*)tableauConfig[i].adresse) = Serial.read();
//                    break;
//                case FLOAT:
//                    *((float*)tableauConfig[i].adresse) = Serial.read();
//                    break;
//                case BOOL:
//                    *((bool*)tableauConfig[i].adresse) = Serial.read();
//                    break;
//                case JDLS: switch (Serial.read())
//                {
//                case MON:
//                    *((JourDeLaSemaine*)tableauConfig[i].adresse) = LUNDI;
//                    clock.fillDayOfWeek(MON);
//                    break;
//                case TUE:
//                    *((JourDeLaSemaine*)tableauConfig[i].adresse) = MARDI;
//                    clock.fillDayOfWeek(TUE);
//                    break;
//                case WED:
//                    *((JourDeLaSemaine*)tableauConfig[i].adresse) = MERCREDI;
//                    clock.fillDayOfWeek(WED);
//                    break;
//                case THU:
//                    *((JourDeLaSemaine*)tableauConfig[i].adresse) = JEUDI;
//                    clock.fillDayOfWeek(THU);
//                    break;
//                case FRI:
//                    *((JourDeLaSemaine*)tableauConfig[i].adresse) = VENDREDI;
//                    clock.fillDayOfWeek(FRI);
//                    break;
//                case SAT:
//                    *((JourDeLaSemaine*)tableauConfig[i].adresse) = SAMEDI;
//                    clock.fillDayOfWeek(SAT);
//                    break;
//                case SUN:
//                    *((JourDeLaSemaine*)tableauConfig[i].adresse) = DIMANCHE;
//                    clock.fillDayOfWeek(SUN);
//                    break;
//                default:
//                    break;
//                }
//                default:
//                    break;
//                }
//                verif = true;
//            }
//            lastInBytes = inBytes;
//            currentMillis = millis();
//            duree = currentMillis - previousMillis;
//        }
//        verif = false;
//        lastInBytes = inBytes;
//        if (duree >= 180000)
//        {
//            *modeAncien = *modeActuel;
//            *modeActuel = STANDARD;
//            changeModeSys(modeActuel, modeAncien, Actuel);
//        }
//    }
//
//    clock.fillByYMD(Actuel->Temps.Annee, Actuel->Temps.Mois, Actuel->Temps.Jour);
//    clock.fillByHMS(Actuel->Temps.Heure, Actuel->Temps.Minute, Actuel->Temps.Seconde);
//    clock.setTime();
//
//    *modeAncien = *modeActuel;
//    *modeActuel = STANDARD;
//    changeModeSys(modeActuel, modeAncien, Actuel);
}

void standard(ModeSys* modeAncien, ModeSys* modeActuel, Parametre* Actuel)
{

    allumerLEDS(0, 255, 0);
    if (*modeAncien == ECONOMIQUE)
    {
        Actuel->Log_Intervall = Actuel->Log_Intervall / 2;
    }

}       //permet au systeme de passer en mode standard

void economique(ModeSys* modeAncien, ModeSys* modeActuel, Parametre* Actuel)
{
    allumerLEDS(0, 0, 255);
    if (*modeAncien == STANDARD)
    {
        Actuel->Log_Intervall = Actuel->Log_Intervall * 2;
    }
}       //permet au systeme de passer en mode economique

void maintenance()
{

    allumerLEDS(255, 128, 0);

}       //permet au systeme de passer en maintenance

void changeModeSys(ModeSys* modeActuel, ModeSys* modeAncien, Parametre* Actuel)
{

    switch (*modeActuel)
    {
    case STANDARD: standard(modeAncien, modeActuel, Actuel);
        break;
    case ECONOMIQUE: economique(modeAncien, modeActuel, Actuel);
        break;
    case CONFIGURATION: configuration(modeAncien, modeActuel, Actuel);
        break;
    case MAINTENANCE: maintenance();
        break;
    default:
        break;
    }

}       //permet au systeme de passer entre differents mode simplement

/* ------------------------------------------------------------------------------------ */

void afficheMesure(Mesure* mesure)
{

    Serial.print(F("Temperature :"));
    Serial.println(mesure->Temperature);

    Serial.print(F("Position :\nN :"));
    Serial.print(mesure->Position.TestN);
    Serial.print(F(" E :"));
    Serial.print(mesure->Position.TestE);
    Serial.print(F(" M :"));
    Serial.println(mesure->Position.TestM);

    Serial.print(F("Temps :"));
    Serial.print(mesure->Temps.Heure);
    Serial.print(F(":"));
    Serial.print(mesure->Temps.Minute);
    Serial.print(F(":"));
    Serial.println(mesure->Temps.Seconde);

    Serial.print(F("Date :"));
    Serial.print(mesure->Temps.JDLS);
    Serial.print(F(" "));
    Serial.print(mesure->Temps.Jour);
    Serial.print(F(" "));
    Serial.print(mesure->Temps.Mois);
    Serial.print(F(" "));
    Serial.println(mesure->Temps.Annee);

    Serial.print(F("Luminosite :"));
    Serial.println(mesure->Lux);

    Serial.print(F("Humidite :"));
    Serial.println(mesure->Hygr);

    Serial.print(F("Pression :"));
    Serial.println(mesure->Pressure);

}       // affiche la mesure entrée

void mettreDansData(Mesure* temporaire, String* data)
{
    String jour;
    switch (temporaire->Temps.JDLS)
    {
    case LUNDI :
        jour = F("Lundi");
        break;
    case MARDI : 
        jour = F("Mardi");
        break;
    case MERCREDI :
        jour = F("Mercredi");
        break;
    case JEUDI :
        jour = F("Jeudi");
        break;
    case VENDREDI :
        jour = F("Vendredi");
        break;
    case SAMEDI :
        jour = F("Samedi");
        break;
    case DIMANCHE :
        jour = F("Dimanche");
        break;
    default:
        break;
    }
    *data = String(temporaire->Temperature, 1) + ", " + String(temporaire->Position.TestN, 4) + ", " + String(temporaire->Position.TestE, 4) + ", " + String(temporaire->Position.TestM, 2) + ", " + String(temporaire->Temps.Heure) + ", " + String(temporaire->Temps.Minute) + ", " + String(temporaire->Temps.Seconde) + ", " + jour + ", " + String(temporaire->Temps.Jour) + ", " + String(temporaire->Temps.Mois) + ", " + String(temporaire->Temps.Annee) + ", " + String(temporaire->Lux) + ", " + String(temporaire->Hygr) + ", " + String(temporaire->Pressure);
}

//void UpdateMoyGlissante(Mesure* moyenneGlissante, Mesure* mesure, short* nbMesure)
//{
//
//    short nbMesureTemp = *nbMesure + 1;
//    // ne pas mettre de if>1 , on perd du temps a la repitition
//    moyenneGlissante->Hygr = moyenneGlissante->Hygr * (nbMesureTemp - 1);
//    moyenneGlissante->Lux = moyenneGlissante->Lux * (nbMesureTemp - 1);
//    moyenneGlissante->Temperature = moyenneGlissante->Temperature * (nbMesureTemp - 1);
//    moyenneGlissante->Pressure = moyenneGlissante->Pressure * (nbMesureTemp - 1);
//
//    moyenneGlissante->Hygr += mesure->Hygr;
//    moyenneGlissante->Lux += mesure->Lux;
//    moyenneGlissante->Temperature += mesure->Temperature;
//    moyenneGlissante->Pressure += mesure->Pressure;
//
//    // ne pas mettre de if>1 , on perd du temps a la repetition
//    moyenneGlissante->Hygr = moyenneGlissante->Hygr / nbMesureTemp;
//    moyenneGlissante->Lux = moyenneGlissante->Lux / nbMesureTemp;
//    moyenneGlissante->Temperature = moyenneGlissante->Temperature / nbMesureTemp;
//    moyenneGlissante->Pressure = moyenneGlissante->Pressure / nbMesureTemp;
//
//    afficheMesure(moyenneGlissante);
//
//}

void allumerLEDS(unsigned char setColorR, unsigned char setColorG, unsigned char setColorB)
{
    leds.setColorRGB(0, setColorR, setColorG, setColorB);
}

/* ------------------------------------------------------------------------------------ */

void setup()
{

    randomSeed(analogRead(0));      // pour simuler les capteurs
    Serial.begin(9600);             //debut du port Serie
    while (!Serial) {};

    pinMode(LIGHTPIN, INPUT);       //definition des pins
    pinMode(BUTTONRED, INPUT);
    pinMode(BUTTONGREEN, INPUT);

    clock.begin();                  //configuration de la clock
    clock.fillByYMD(2020, 1, 1);
    clock.fillByHMS(00, 00, 00);
    clock.fillDayOfWeek(WED);
    clock.setTime();

    attachInterrupt(digitalPinToInterrupt(BUTTONRED), boutonRougeAppuye, FALLING);      //mise en place des interrupts
    attachInterrupt(digitalPinToInterrupt(BUTTONGREEN), boutonVertAppuye, FALLING);

}

/* ------------------------------------------------------------------------------------ */

void loop()
{

    allumerLEDS(0, 255, 0);
    Serial.println(F("Pensez à configurer votre systeme"));

    Mesure mesure = { 0 };
    Mesure moyenneGlissante = { 0 };

    Parametre Defaut;
    ValeurDefaut(&Defaut);
    Parametre Actuel = Defaut;

    ModeSys modeActuel = STANDARD;
    ModeSys modeAncien = STANDARD;

    long tempsEntreMesure = 0;
    short nbMesure = -1;
    bool stocker = false;
    bool diviser = false;
    //int nbMesureMax = Actuel.File_Max_Size / sizeof(Mesure);
    int nbMesureMax = 2;
    Mesure* tabMesure = (Mesure*)malloc(sizeof(Mesure) * nbMesureMax);
    String data;
    int TimeChecker1 = 0;
    int TimeChecker2 = clock.minute * 60 + clock.second;

    if ((digitalRead(BUTTONRED)) == LOW)
    {
        modeAncien = modeActuel;
        modeActuel = CONFIGURATION;
        changeModeSys(&modeActuel, &modeAncien, &Actuel);
    }

    ajouterSD(&stocker);

    while (true)
    {
        tempsEntreMesure = TimeChecker1 - TimeChecker2;
        if (hasInterruptR && !hasInterruptV)
        {
            hasInterruptR = false;
            attachInterrupt(digitalPinToInterrupt(BUTTONRED), boutonRougeAppuye, FALLING);
            if ((temp2 - temp) >= 5000)
            {
                temp2 = 0;
                if (modeActuel == MAINTENANCE)
                {
                    modeActuel = modeAncien;
                    modeAncien = MAINTENANCE;
                    ajouterSD(&stocker);
                    changeModeSys(&modeActuel, &modeAncien, &Actuel);
                }
                else
                {
                    modeAncien = modeActuel;
                    modeActuel = MAINTENANCE;
                    retirerSD(&stocker);
                    changeModeSys(&modeActuel, &modeAncien, &Actuel);
                }
            }
            else
            {
                temp2 = 0;
            }
        }

        if (!hasInterruptR && hasInterruptV)
        {
            hasInterruptV = false;
            attachInterrupt(digitalPinToInterrupt(BUTTONGREEN), boutonVertAppuye, FALLING);
            if ((temp2 - temp) >= 5000)
            {
                temp2 = 0;
                if (modeActuel == ECONOMIQUE)
                {
                    modeAncien = modeActuel;
                    modeActuel = STANDARD;
                    changeModeSys(&modeActuel, &modeAncien, &Actuel);
                }
                else
                {
                    modeAncien = modeActuel;
                    modeActuel = ECONOMIQUE;
                    changeModeSys(&modeActuel, &modeAncien, &Actuel);
                }
            }
            else
            {
                temp2 = 0;
            }
        }

        if (tempsEntreMesure >= (Actuel.Log_Intervall * 0.001))
        {

            if (Actuel.Temp_Air == true)
                obtenirTemperature(&mesure.Temperature, &Actuel);

            if (Actuel.Lumin == true)
                obtenirLuminosite(&mesure.Lux, &Actuel);

            if (Actuel.Hygr == true)
                obtenirHumidite(&mesure.Hygr, &Actuel);

            if (Actuel.Pressure == true)
                obtenirPression(&mesure.Pressure, &Actuel);

            obtenirGPS(&mesure.Position, &diviser, &modeActuel);
            obtenirTemps(&mesure.Temps, &TimeChecker2);
            TimeChecker2 = clock.minute * 60 + clock.second;

            if (stocker)
            {
                nbMesure++;
                tabMesure[nbMesure] = mesure;
                //UpdateMoyGlissante(&moyenneGlissante, &mesure, &nbMesure);
                memset(&mesure, sizeof(Mesure), 0);
            }
            else
            {
                afficheMesure(&mesure);
                memset(&mesure, sizeof(Mesure), 0);
            }
            
            if (nbMesure == nbMesureMax)
            {
                for (int i = 0; i < nbMesureMax; i++)       //ecrire dans le fichier
                {
                    data = "";
                    mettreDansData(&tabMesure[i], &data);

                    // fichier txt qui contient les donn�es 
                if (SD.exists(filename)) {
                  
                  }else {
                    dataFile = SD.open(filename, FILE_WRITE);
                    dataFile.close();
                }
                
                
                if (SD.exists(filename)) {
                    dataFile = SD.open(filename, FILE_WRITE);
                    dataFile.println(data);
                    dataFile.close();
                }
                else {
                    boucleInfiniErreur(255, 0, 0, 0, 0, 255, 500);
                }
                
                 memset(&mesure, sizeof(Mesure),0);
                 //memset(&moyenneGlissante, sizeof(Mesure), 0);
                 memset(&tabMesure, sizeof(Mesure)*nbMesureMax, 0);
                 nbMesure = -1;
                 

            }
        }

        }
        clock.getTime();
        TimeChecker1 = clock.minute * 60 + clock.second;
}
}
