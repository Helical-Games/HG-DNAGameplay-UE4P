// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "DNATagContainer.h"
#include "DNATagAssetInterface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "BlueprintDNATagLibrary.generated.h"

UCLASS(MinimalAPI)
class UBlueprintDNATagLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Determine if TagOne matches against TagTwo
	 * 
	 * @param TagOne			Tag to check for match
	 * @param TagTwo			Tag to check match against
	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagOne will include it's parent tags while matching			
	 * 
	 * @return True if TagOne matches TagTwo
	 */
	UFUNCTION(BlueprintPure, Category="DNATags", meta = (Keywords = "DoDNATagsMatch"))
	static bool MatchesTag(FDNATag TagOne, FDNATag TagTwo, bool bExactMatch);

	/**
	 * Determine if TagOne matches against any tag in OtherContainer
	 * 
	 * @param TagOne			Tag to check for match
	 * @param OtherContainer	Container to check against.
	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagOne will include it's parent tags while matching
	 * 
	 * @return True if TagOne matches any tags explicitly present in OtherContainer
	 */
	UFUNCTION(BlueprintPure, Category="DNATags")
	static bool MatchesAnyTags(FDNATag TagOne, const FDNATagContainer& OtherContainer, bool bExactMatch);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (DNATag)", CompactNodeTitle="=="), Category="DNATags")
	static bool EqualEqual_DNATag( FDNATag A, FDNATag B );
	
	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (DNATag)", CompactNodeTitle="!="), Category="DNATags")
	static bool NotEqual_DNATag( FDNATag A, FDNATag B );

	/** Returns true if the passed in DNA tag is non-null */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static bool IsDNATagValid(FDNATag DNATag);

	/** Returns FName of this tag */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FName GetTagName(const FDNATag& DNATag);

	/** Creates a literal FDNATag */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FDNATag MakeLiteralDNATag(FDNATag Value);

	/**
	 * Get the number of DNA tags in the specified container
	 * 
	 * @param TagContainer	Tag container to get the number of tags from
	 * 
	 * @return The number of tags in the specified container
	 */
	UFUNCTION(BlueprintPure, Category="DNATags")
	static int32 GetNumDNATagsInContainer(const FDNATagContainer& TagContainer);

	/**
	 * Check if the tag container has the specified tag
	 *
	 * @param TagContainer			Container to check for the tag
	 * @param Tag					Tag to check for in the container
	 * @param bExactMatch			If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching			
	 *
	 * @return True if the container has the specified tag, false if it does not
	 */
	UFUNCTION(BlueprintPure, Category="DNATags", meta = (Keywords = "DoesContainerHaveTag"))
	static bool HasTag(const FDNATagContainer& TagContainer, FDNATag Tag, bool bExactMatch);

	/**
	 * Check if the specified tag container has ANY of the tags in the other container
	 * 
	 * @param TagContainer			Container to check if it matches any of the tags in the other container
	 * @param OtherContainer		Container to check against.
	 * @param bExactMatch			If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching			
	 * 
	 * @return True if the container has ANY of the tags in the other container
	 */
	UFUNCTION(BlueprintPure, Category="DNATags", meta = (Keywords = "DoesContainerMatchAnyTagsInContainer"))
	static bool HasAnyTags(const FDNATagContainer& TagContainer, const FDNATagContainer& OtherContainer, bool bExactMatch);

	/**
	 * Check if the specified tag container has ALL of the tags in the other container
	 * 
	 * @param TagContainer			Container to check if it matches all of the tags in the other container
	 * @param OtherContainer		Container to check against. If this is empty, the check will succeed
	 * @param bExactMatch			If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching			
	 * 
	 * @return True if the container has ALL of the tags in the other container
	 */
	UFUNCTION(BlueprintPure, Category="DNATags", meta = (Keywords = "DoesContainerMatchAllTagsInContainer"))
	static bool HasAllTags(const FDNATagContainer& TagContainer, const FDNATagContainer& OtherContainer, bool bExactMatch);

	/**
	 * Check if the specified tag container matches the given Tag Query
	 * 
	 * @param TagContainer			Container to check if it matches all of the tags in the other container
	 * @param TagQuery				Query to match against
	 * 
	 * @return True if the container matches the query, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static bool DoesContainerMatchTagQuery(const FDNATagContainer& TagContainer, const FDNATagQuery& TagQuery);

	/**
	 * Get an array of all actors of a specific class (or subclass of that class) which match the specified DNA tag query.
	 * 
	 * @param ActorClass			Class of actors to fetch
	 * @param DNATagQuery		Query to match against
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category="DNATags",  meta=(WorldContext="WorldContextObject", DeterminesOutputType="ActorClass", DynamicOutputParam="OutActors"))
	static void GetAllActorsOfClassMatchingTagQuery(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FDNATagQuery& DNATagQuery, TArray<AActor*>& OutActors);

	/**
	 * Adds a single tag to the passed in tag container
	 *
	 * @param InOutTagContainer		The container that will be appended too.
	 * @param Tag					The tag to add to the container
	 */
	UFUNCTION(BlueprintCallable, Category = "DNATags")
	static void AddDNATag(UPARAM(ref) FDNATagContainer& InOutTagContainer, FDNATag Tag);

	/**
	 * Appends all tags in the InTagContainer to InOutTagContainer
	 *
	 * @param InOutTagContainer		The container that will be appended too.
	 * @param InTagContainer		The container to append.
	 */
	UFUNCTION(BlueprintCallable, Category = "DNATags")
	static void AppendDNATagContainers(UPARAM(ref) FDNATagContainer& InOutTagContainer, const FDNATagContainer& InTagContainer);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (DNATagContainer)", CompactNodeTitle="=="), Category="DNATags")
	static bool EqualEqual_DNATagContainer( const FDNATagContainer& A, const FDNATagContainer& B );
	
	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (DNATagContainer)", CompactNodeTitle="!="), Category="DNATags")
	static bool NotEqual_DNATagContainer( const FDNATagContainer& A, const FDNATagContainer& B );

	/** Creates a literal FDNATagContainer */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FDNATagContainer MakeLiteralDNATagContainer(FDNATagContainer Value);

	/** Creates a FDNATagContainer from the array of passed in tags */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FDNATagContainer MakeDNATagContainerFromArray(const TArray<FDNATag>& DNATags);

	/** Creates a FDNATagContainer containing a single tag */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FDNATagContainer MakeDNATagContainerFromTag(FDNATag SingleTag);

	/** Breaks tag container into explicit array of tags */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static void BreakDNATagContainer(const FDNATagContainer& DNATagContainer, TArray<FDNATag>& DNATags);

	/**
	 * Creates a literal FDNATagQuery
	 *
	 * @param	TagQuery	value to set the FDNATagQuery to
	 *
	 * @return	The literal FDNATagQuery
	 */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FDNATagQuery MakeDNATagQuery(FDNATagQuery TagQuery);

	/**
	 * Check DNA tags in the interface has all of the specified tags in the tag container (expands to include parents of asset tags)
	 *
	 * @param TagContainerInterface		An Interface to a tag container
	 * @param OtherContainer			A Tag Container
	 *
	 * @return True if the tagcontainer in the interface has all the tags inside the container.
	 */
	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool HasAllMatchingDNATags(TScriptInterface<IDNATagAssetInterface> TagContainerInterface, const FDNATagContainer& OtherContainer);

	/**
	 * Check if the specified tag container has the specified tag, using the specified tag matching types
	 *
	 * @param TagContainerInterface		An Interface to a tag container
	 * @param Tag						Tag to check for in the container
	 *
	 * @return True if the container has the specified tag, false if it does not
	 */
	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool DoesTagAssetInterfaceHaveTag(TScriptInterface<IDNATagAssetInterface> TagContainerInterface, FDNATag Tag);

	/** Checks if a DNA tag's name and a string are not equal to one another */
	UFUNCTION(BlueprintPure, Category = PinOptions, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool NotEqual_TagTag(FDNATag A, FString B);

	/** Checks if a DNA tag containers's name and a string are not equal to one another */
	UFUNCTION(BlueprintPure, Category = PinOptions, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool NotEqual_TagContainerTagContainer(FDNATagContainer A, FString B);
	
	/**
	 * Returns an FString listing all of the DNA tags in the tag container for debugging purposes.
	 *
	 * @param TagContainer	The tag container to get the debug string from.
	 */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FString GetDebugStringFromDNATagContainer(const FDNATagContainer& TagContainer);

	/**
	 * Returns an FString representation of a DNA tag for debugging purposes.
	 *
	 * @param DNATag	The tag to get the debug string from.
	 */
	UFUNCTION(BlueprintPure, Category = "DNATags")
	static FString GetDebugStringFromDNATag(FDNATag DNATag);

};
