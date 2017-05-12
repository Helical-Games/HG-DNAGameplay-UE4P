// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ContentBrowserFrontEndFilterExtension.h"
#include "DNATagSearchFilter.generated.h"

UCLASS()
class UDNATagSearchFilter : public UContentBrowserFrontEndFilterExtension
{
public:
	GENERATED_BODY()

	// UContentBrowserFrontEndFilterExtension interface
	virtual void AddFrontEndFilterExtensions(TSharedPtr<class FFrontendFilterCategory> DefaultCategory, TArray< TSharedRef<class FFrontendFilter> >& InOutFilterList) const override;
	// End of UContentBrowserFrontEndFilterExtension interface
};
