// Fill out your copyright notice in the Description page of Project Settings.


#include "Code/Actors/TeleportPoint.h"

// Sets default values
ATeleportPoint::ATeleportPoint()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATeleportPoint::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATeleportPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

