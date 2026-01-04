// TooltipWidget.cpp
// Implementation of tooltip widget text management
// Developer: Marcus Daley
// Date: December 31, 2025

#include "Code/UI/TooltipWidget.h"
#include "Components/TextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogTooltipWidget, Log, All);

void UTooltipWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!ValidateWidgets())
    {
        UE_LOG(LogTooltipWidget, Error,
            TEXT("[TooltipWidget] Validation failed - Check BindWidget names!"));
    }

    UE_LOG(LogTooltipWidget, Log, TEXT("[TooltipWidget] Constructed successfully"));
}

void UTooltipWidget::SetDisplayText(const FText& NewText)
{
    if (!DisplayText)
    {
        UE_LOG(LogTooltipWidget, Error, TEXT("[TooltipWidget] DisplayText widget not bound!"));
        return;
    }

    DisplayText->SetText(NewText);
}

void UTooltipWidget::SetInteractionPrompt(const FText& NewText)
{
    if (!InteractionPromptText)
    {
        UE_LOG(LogTooltipWidget, Error,
            TEXT("[TooltipWidget] InteractionPromptText widget not bound!"));
        return;
    }

    // Hide prompt if empty, show if has text
    if (NewText.IsEmpty())
    {
        InteractionPromptText->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        InteractionPromptText->SetText(NewText);
        InteractionPromptText->SetVisibility(ESlateVisibility::Visible);
    }
}

bool UTooltipWidget::ValidateWidgets() const
{
    bool bValid = true;

    if (!DisplayText)
    {
        UE_LOG(LogTooltipWidget, Error,
            TEXT("[TooltipWidget] DisplayText widget is null - Check BindWidget name in Blueprint!"));
        bValid = false;
    }

    if (!InteractionPromptText)
    {
        UE_LOG(LogTooltipWidget, Error,
            TEXT("[TooltipWidget] InteractionPromptText widget is null - Check BindWidget name in Blueprint!"));
        bValid = false;
    }

    return bValid;
}