// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class INTAKEPROTOCOL_API InmatesNameEnum
{
public:
	InmatesNameEnum();
	~InmatesNameEnum();
};

UENUM(BlueprintType)
enum class E_InmatesName : uint8
{
    JohnSmith UMETA(DisplayName = "JohnSmith"),
    MichaelJohnson UMETA(DisplayName = "MichaelJohnson"),
    DavidBrown UMETA(DisplayName = "DavidBrown"),
    JamesDavis UMETA(DisplayName = "JamesDavis"),
    RobertMiller UMETA(DisplayName = "RobertMiller"),
    DanielWilson UMETA(DisplayName = "DanielWilson"),
    WilliamMoore UMETA(DisplayName = "WilliamMoore"),
    AlexTaylor UMETA(DisplayName = "AlexTaylor"),
    VictorAnderson UMETA(DisplayName = "VictorAnderson"),
    RyanThomas UMETA(DisplayName = "RyanThomas"),
    LucasJackson UMETA(DisplayName = "LucasJackson"),
    EthanWhite UMETA(DisplayName = "EthanWhite"),
    OliverHarris UMETA(DisplayName = "OliverHarris"),
    NoahMartin UMETA(DisplayName = "NoahMartin"),
    LiamThompson UMETA(DisplayName = "LiamThompson"),
    HenryGarcia UMETA(DisplayName = "HenryGarcia"),
    JackMartinez UMETA(DisplayName = "JackMartinez"),
    LeoRobinson UMETA(DisplayName = "LeoRobinson"),
    OscarClark UMETA(DisplayName = "OscarClark"),
    AdamLewis UMETA(DisplayName = "AdamLewis")
};

UENUM(BlueprintType)
enum class E_Country : uint8
{
    Montara UMETA(DisplayName = "Montara"),
    Svetos UMETA(DisplayName = "Svetos"),
    Winland UMETA(DisplayName = "Winland"),
    Asmanistan UMETA(DisplayName = "Asmanistan")
};

UENUM(BlueprintType)
enum class E_Charge : uint8
{
    Murder UMETA(DisplayName = "Murder"),
    Theft UMETA(DisplayName = "Theft"),
    ArmedRobbery UMETA(DisplayName = "Armed Robbery"),
    DrugTrafficking UMETA(DisplayName = "Drug Trafficking"),
    DrugPossession UMETA(DisplayName = "Drug Possession"),
    Fraud UMETA(DisplayName = "Fraud"),
    MoneyLaundering UMETA(DisplayName = "Money Laundering"),
    Assault UMETA(DisplayName = "Assault"),
    AggravatedAssault UMETA(DisplayName = "Aggravated Assault"),
    Kidnapping UMETA(DisplayName = "Kidnapping"),
    HumanTrafficking UMETA(DisplayName = "Human Trafficking"),
    IllegalFirearmPossession UMETA(DisplayName = "Illegal Firearm Possession"),
    Extortion UMETA(DisplayName = "Extortion"),
    Cybercrime UMETA(DisplayName = "Cybercrime"),
    Burglary UMETA(DisplayName = "Burglary"),
    Vandalism UMETA(DisplayName = "Vandalism"),
    Bribery UMETA(DisplayName = "Bribery"),
    TaxEvasion UMETA(DisplayName = "Tax Evasion"),
    Smuggling UMETA(DisplayName = "Smuggling"),
    AttemptedMurder UMETA(DisplayName = "Attempted Murder"),
    DomesticViolence UMETA(DisplayName = "Domestic Violence")
};