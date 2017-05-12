// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SUserWidget.h"

/** Main CollisionAnalyzer UI widget */
class SDNACueEditor : public SUserWidget
{
public:

	SLATE_USER_ARGS(SDNACueEditor) {}
	SLATE_END_ARGS()
	virtual void Construct(const FArguments& InArgs) = 0;

	virtual void OnNewDNACueTagCommited(const FText& InText, ETextCommit::Type InCommitType) = 0;
	virtual void OnSearchTagCommited(const FText& InText, ETextCommit::Type InCommitType) = 0;
	virtual void HandleNotify_OpenAssetInEditor(FString AssetName, int AssetType) = 0;
	virtual void HandleNotify_FindAssetInEditor(FString AssetName, int AssetType) = 0;

	virtual FReply OnNewDNACueButtonPressed() = 0;
	
	static FString GetPathNameForDNACueTag(FString Tag);

	static void CreateNewDNACueNotifyDialogue(FString DNACue, UClass* ParentClass);
	static void OpenEditorForNotify(FString NotifyFullPath);
};
