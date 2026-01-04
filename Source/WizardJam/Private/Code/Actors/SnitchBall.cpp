// Fill out your copyright notice in the Description page of Project Settings.


#include "Code/Actors/SnitchBall.h"

// Sets default values
ASnitchBall::ASnitchBall()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASnitchBall::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASnitchBall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ASnitchBall::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

