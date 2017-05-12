// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Engine/EngineTypes.h"

class FCanvasItem;
class UCanvas;
class UFont;
struct FCanvasIcon;

DECLARE_LOG_CATEGORY_EXTERN(LogDNADebug, Log, All);

class FCanvasItem;
struct FCanvasIcon;

class DNADEBUGGER_API FDNADebuggerCanvasContext
{
public:
	/** canvas used for drawing */
	TWeakObjectPtr<UCanvas> Canvas;

	/** current text font */
	TWeakObjectPtr<UFont> Font;

	/** font render data */
	FFontRenderInfo FontRenderInfo;

	/** position of cursor */
	float CursorX, CursorY;

	/** default position of cursor */
	float DefaultX, DefaultY;

	FDNADebuggerCanvasContext() {}
	FDNADebuggerCanvasContext(UCanvas* InCanvas, UFont* InFont);

	// print string on canvas
	void Print(const FString& String);
	void Print(const FColor& Color, const FString& String);
	void PrintAt(float PosX, float PosY, const FColor& Color, const FString& String);
	void PrintAt(float PosX, float PosY, const FString& String);

	// print formatted string on canvas
	VARARG_DECL(void, void, {}, Printf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE);
	VARARG_DECL(void, void, {}, Printf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(Color));
	VARARG_DECL(void, void, {}, PrintfAt, VARARG_NONE, const TCHAR*, VARARG_EXTRA(float PosX) VARARG_EXTRA(float PosY), VARARG_EXTRA(PosX) VARARG_EXTRA(PosY));
	VARARG_DECL(void, void, {}, PrintfAt, VARARG_NONE, const TCHAR*, VARARG_EXTRA(float PosX) VARARG_EXTRA(float PosY) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(PosX) VARARG_EXTRA(PosY) VARARG_EXTRA(Color));

	// moves cursor to new line
	void MoveToNewLine();

	// calculate size of string
	void MeasureString(const FString& String, float& OutSizeX, float& OutSizeY) const;

	// get height of single line text
	float GetLineHeight() const;

	// project world location on canvas
	FVector2D ProjectLocation(const FVector& Location) const;

	// check if world location is visible in current view
	bool IsLocationVisible(const FVector& Location) const;

	// draw item on canvas
	void DrawItem(FCanvasItem& Item, float PosX, float PosY);

	// draw icon on canvas
	void DrawIcon(const FColor& Color, const FCanvasIcon& Icon, float PosX, float PosY, float Scale = 1.f);
};

namespace FDNADebuggerCanvasStrings
{
	static FString ColorNameInput = TEXT("white");
	static FString ColorNameEnabled = TEXT("green");
	static FString ColorNameDisabled = TEXT("grey");
	static FString ColorNameEnabledActiveRow = TEXT("green");
	static FString ColorNameDisabledActiveRow = TEXT("black");

	static FString Separator = TEXT("{white} | ");
	static FString SeparatorSpace = TEXT("  ");
}

enum class EDNADebuggerShape : uint8
{
	Invalid,
	Point,
	Segment,
	Box,
	Cone,
	Cylinder,
	Capsule,
	Polygon,
};

struct DNADEBUGGER_API FDNADebuggerShape
{
	/** points defining shape */
	TArray<FVector> ShapeData;

	/** description of shape */
	FString Description;

	/** color of shape */
	FColor Color;

	/** type of shape */
	EDNADebuggerShape Type;

	FDNADebuggerShape() : Type(EDNADebuggerShape::Invalid) {}

	bool operator==(const FDNADebuggerShape& Other) const
	{
		return (Type == Other.Type) && (Color == Other.Color) && (Description == Other.Description) && (ShapeData == Other.ShapeData);
	}

	static FDNADebuggerShape MakePoint(const FVector& Location, const float Radius, const FColor& Color, const FString& Description = FString());
	static FDNADebuggerShape MakeSegment(const FVector& StartLocation, const FVector& EndLocation, const float Thickness, const FColor& Color, const FString& Description = FString());
	static FDNADebuggerShape MakeSegment(const FVector& StartLocation, const FVector& EndLocation, const FColor& Color, const FString& Description = FString());
	static FDNADebuggerShape MakeBox(const FVector& Center, const FVector& Extent, const FColor& Color, const FString& Description = FString());
	static FDNADebuggerShape MakeCone(const FVector& Location, const FVector& Direction, const float Length, const FColor& Color, const FString& Description = FString());
	static FDNADebuggerShape MakeCylinder(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description = FString());
	static FDNADebuggerShape MakeCapsule(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description = FString());
	static FDNADebuggerShape MakePolygon(const TArray<FVector>& Verts, const FColor& Color, const FString& Description = FString());

	void Draw(UWorld* World, FDNADebuggerCanvasContext& Context);
};

FArchive& operator<<(FArchive& Ar, FDNADebuggerShape& Shape);

enum class EDNADebuggerDataPack : uint8
{
	Persistent,
	ResetOnActorChange,
	ResetOnTick,
};

struct DNADEBUGGER_API FDNADebuggerDataPack
{
	struct FHeader
	{
		/** version, increased every time new Data is requested */
		int16 DataVersion;

		/** debug actor sync counter */
		int16 SyncCounter;

		/** size of Data array */
		int32 DataSize;

		/** offset to currently replicated portion of data */
		int32 DataOffset;

		/** is data compressed? */
		uint32 bIsCompressed : 1;

		FHeader() : DataVersion(0), SyncCounter(0), DataSize(0), DataOffset(0), bIsCompressed(false) {}

		FORCEINLINE bool Equals(const FHeader& Other) const
		{
			return (DataVersion == Other.DataVersion) && (DataSize == Other.DataSize) && (DataOffset == Other.DataOffset);
		}

		FORCEINLINE bool operator==(const FHeader& Other) const
		{
			return Equals(Other);
		}

		FORCEINLINE bool operator!=(const FHeader& Other) const
		{
			return !Equals(Other);
		}
	};

	DECLARE_DELEGATE(FOnReset);
	DECLARE_DELEGATE_OneParam(FOnSerialize, FArchive&);

	FDNADebuggerDataPack() :
		PackId(0), DataCRC(0), bIsDirty(false), bNeedsConfirmation(false), bReceived(false),
		Flags(EDNADebuggerDataPack::ResetOnTick)
	{}

	/** data being replicated */
	TArray<uint8> Data;

	/** minimal header used for DataPack state comparison */
	FHeader Header;

	/** auto assigned Id of pack */
	int32 PackId;

	/** CRC used to test changes in Data array */
	uint32 DataCRC;

	/** force net replication, regardless DataCRC */
	uint32 bIsDirty : 1;

	/** if set, replication must be confirmed by client before sending next update */
	uint32 bNeedsConfirmation : 1;

	/** set when client receives complete data pack */
	uint32 bReceived : 1;

	/** data pack flags */
	EDNADebuggerDataPack Flags;

	FOnReset ResetDelegate;
	FOnSerialize SerializeDelegate;

	static int32 PacketSize;

	bool CheckDirtyAndUpdate();
	bool RequestReplication(int16 SyncCounter);

	void OnReplicated();
	void OnPacketRequest(int16 DataVersion, int32 DataOffset);

	/** get replication progress in (0..1) range */
	FORCEINLINE float GetProgress() const
	{
		return (Header.DataOffset != Header.DataSize) ? ((1.0f * Header.DataOffset) / Header.DataSize) : 1.0f;
	}

	/** is replication in progress? */
	FORCEINLINE bool IsInProgress() const
	{
		return (Header.DataSize > 0) && (Header.DataOffset < Header.DataSize);
	}

	static bool IsMultiPacket(int32 TestSize)
	{
		return TestSize > PacketSize;
	}
};

enum class EDNADebuggerInputMode : uint8
{
	// input handler is called on local category
	Local,

	// input handler is replicated to authority category and called there
	Replicated,
};

struct DNADEBUGGER_API FDNADebuggerInputModifier
{
	uint32 bShift : 1;
	uint32 bCtrl : 1;
	uint32 bAlt : 1;
	uint32 bCmd : 1;
	uint32 bPressed : 1;
	uint32 bReleased : 1;

	FDNADebuggerInputModifier() : bShift(false), bCtrl(false), bAlt(false), bCmd(false), bPressed(true), bReleased(false) {}
	FDNADebuggerInputModifier(bool bInShift, bool bInCtrl, bool bInAlt, bool bInCmd) : bShift(bInShift), bCtrl(bInCtrl), bAlt(bInAlt), bCmd(bInCmd), bPressed(true), bReleased(false) {}

	FDNADebuggerInputModifier operator+(const FDNADebuggerInputModifier& Other) const
	{
		return FDNADebuggerInputModifier(bShift || Other.bShift, bCtrl || Other.bCtrl, bAlt || Other.bAlt, bCmd || Other.bCmd);
	}

	static FDNADebuggerInputModifier Shift;
	static FDNADebuggerInputModifier Ctrl;
	static FDNADebuggerInputModifier Alt;
	static FDNADebuggerInputModifier Cmd;
	static FDNADebuggerInputModifier None;
};

struct DNADEBUGGER_API FDNADebuggerInputHandler
{
	DECLARE_DELEGATE(FHandler);

	FName KeyName;
	FDNADebuggerInputModifier Modifier;
	FHandler Delegate;
	EDNADebuggerInputMode Mode;

	FDNADebuggerInputHandler() : KeyName(NAME_None), Mode(EDNADebuggerInputMode::Local) {}

	bool IsValid() const;
	FString ToString() const;
};

/** 
 * Customizable key binding used by FDNADebuggerAddonBase (both categories and extensions)
 * Intended to use only from addon's constructor!
 * Check example in FDNADebuggerExtension_Spectator::FDNADebuggerExtension_Spectator() for details
 */
struct DNADEBUGGER_API FDNADebuggerInputHandlerConfig
{
	FName KeyName;
	FDNADebuggerInputModifier Modifier;

	FDNADebuggerInputHandlerConfig() : KeyName(NAME_None) {}
	FDNADebuggerInputHandlerConfig(const FName ConfigName, const FName DefaultKeyName);
	FDNADebuggerInputHandlerConfig(const FName ConfigName, const FName DefaultKeyName, const FDNADebuggerInputModifier& DefaultModifier);

	static FName CurrentCategoryName;
	static FName CurrentExtensionName;

private:
	void UpdateConfig(const FName ConfigName);
};
