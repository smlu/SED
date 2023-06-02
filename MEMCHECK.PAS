(*
MemCheck: the ultimate memory troubles hunter
Created by: Jean Marc Eber & Vincent Mahon, Société Générale, INFI/SGOP/R&D
Version 2.00
Changes:
*** 24 August 1998
We use the debug informations found in the executable if they are available
Note: there were probably lots of changes between February and August, but we forgot to update this list
*** 23 February 1998
MemCheckLogFileName is an exported variable, so you can set it at run-time 
*** 20 February 1998
New option: CheckHeapStatus
At the end of each call to the memory manager, we get the heap status
At each entry into a method of the memory manager, we verify that the heap status has not changed (the contrary
would mean something has gone bad and has corrupted the memory).
When this options is turned ON, the rest of MemCheck is disabled. It is then impossible to hunt memory leaks. You have
to choose !
Warning: this is VERY VERY time consuming
*** 15 January 1998
New option: IdentifyObjectFields, which attempts an improved reporting which specifies "object dummy is
never destroyed and was likely to be in 3rd field of object ..."
*** 04 December 1997
MemCheck now correctly raises EOutOfMemory when no more memory is available from the system. Documentation slightly improved.
*** 18 September 1997
Works on Delphi 3 (build 5.53), under Windows NT (should work on Win 95, but this was not tested).
This unit is used in real work conditions at Société Générale, on the Opt'It project (> 400000 lines of code).
We have to say it was undocumented until recently (used internaly only), so the doc (the comments
you are reading) is likely to contain errors, and is definitely incomplete. Feel free to contact us for more information.
You'll notice MemCheck does not have a nice interface, but this is not our aim. The role of MemCheck is
to be an efficient and reliable tool; we do not have a lot of time to make it cute to the end user (don't
forget it is a debugging tool).

Contact...
	Jean Marc Eber is at: Jean-Marc.Eber@socgen.com
	Vincent Mahon is at: Vincent.Mahon@socgen.com

	Our address is:
		Tour Société Générale
		Infi/Sgop/R&D
		92987 Paris - La Défense cedex
		France

Copyrights...
The authors grant you the right to modify/change the source code as long as the original authors are mentionned.
Please let us know if you make any improvements, so that we can keep an up to date version. We also welcome
all comments, preferably by email.

Portions of this file (all the code dealing with TD32 debug information) where derived from the following work, with permission.
Reuse of this code in a commercial application is not permitted. The portions are identified by a copyright notice.
> DumpFB.C Borland 32-bit Turbo Debugger dumper (FB09 & FB0A)
> Clive Turvey, Electronics Engineer, July 1998
> Copyright (C) Tenth Planet Software Intl., Clive Turvey 1998. All rights reserved.
> Clive Turvey <clive@tbcnet.com> http://www.tbcnet.com/~clive/vcomwinp.html

Disclaimer...
You use MemCheck at your own risks. This means that you cannot hold the authors or Société Générale to be
responsible for any software\hardware problems you may encounter while using this module.

General information...
MemCheck replaces Delphi's memory manager with a home made one. This one logs information each time memory is
allocated, reallocated or freed. When the program ends, information about memory problems is provided in a log file
and exceptions are raised at problematic points.

Basic use...
Set the MemCheckLogFileName option. Call MemChk when you want to start the memory monitoring. Nothing else to do !
When your program terminates and the finalization is executed, MemCheck will report the problems. This is the
behaviour you'll obtain if you change no option in MemCheck.

Features...
- List of memory spaces not deallocated, and raising of EMemoryLeak exception at the exact place in the source code
- Call stack at allocation time. User chooses to see or not to see this call stack at run time (using ShowCallStack),
  when a EMemoryLeak is raised.
- Tracking of virtual method calls after object's destruction (we change the VMT of objects when they are destroyed)
- Tracking of method calls on an interface while the object attached to the interface has been destroyed
- Checking of writes beyond end of allocated blocks (we put a marker at the end of a block on allocation)
- Fill freed block with a byte (this allows for example to set fields of classes to Nil, or buffers to $FF, or whatever)
- Detect writes in deallocated blocks (we do this by not really deallocating block, and checking them on end - this
  can be time consuming)
- Statistics collection about objects allocation (how many objects of a given class are created ?)
- Time stamps can be indicated and will appear in the output

Options and parameters...
- You can specify the log files names (MemCheckLogFileName)
- It is possible to tell MemCheck that you are instanciating an object in a special way - See doc for
  CheckForceAllocatedType
- Clients can specify the depth of the call stack they want to store (StoredCallStackDepth)


Warnings...
- MemCheck is based on a lot of low-level hacks. Some parts of it will not work on other versions of Delphi
without being revisited (as soon as System has been recompiled, MemCheck is very likely to behave strangely,
because for example the address of InitContext will be bad).
- As leaks are reported on end of execution (finalization of this unit), we need as many finalizations to occur
before memcheck's, so that if so memory is freed in these finalizations, it is not reported as leak. In order to
finalize MemCheck as late as possible, we use a trick to change the order of the list of finalizations. After
MemCheck are finalized only SysUtils, System and SysInit. This implies that we must not use any other unit which
has a finalization. For example, we can not use the unit classes, so we have to implement our own lists and string
lists here. Other memory managing products which are available (found easily on the internet) do not have this
problem because they just rely on putting the unit first in the DPR; but they report incorrect leaks ! MemCheck does
not, as far as we know.
- Some debugging tools exploit the map file to return source location information. We chose not to do that, because
we think the way MemCheck raises exceptions at the good places is better. It is still possible to use "find error"
in Delphi.
- Memcheck is not able to report accurate call stack information about a leak of a class which does not redefine
its constructor. For example, if an instance of TStringList is never deallocated, the call stack MemCheck will
report is not very complete. However, the leak is correctly reported by MemCheck.

*)
unit MemCheck;
{$A+}
{$H+}
interface

	procedure MemChk;
		{Activates MemCheck and resets the allocated blocks stack.
		Warning: the old stack is lost ! - It is the client's duty to commit the
		releasable blocks by calling CommitReleases(AllocatedBlocks)}

	procedure UnMemChk;
		{sets back the memory manager that was installed before MemChk was called
		If MemCheck is not active, this does not matter. The default delphi memory manager is set}

	procedure OutputAllocatedBlocks;
		{Outputs the blocks allocated}

	procedure CommitReleases;
		{really releases the blocks}

	procedure AddTimeStampInformation(const I: string);
		{Logs the given information as associated with the current time stamp
		Requires that MemCheck is active}

	procedure LogSevereExceptions(const WithVersionInfo: string);
		{Activates the exception logger}

	function MemoryBlockCorrupted(P: Pointer): Boolean;
		{Is the given block bad ?
		P is a block you may for example have created with GetMem, or P can be an object.
		Bad means you have written beyond the block's allocated space or the memory for this object was freed.
		If P was allocated before MemCheck was launched, we return False}

	function BlockAllocationAddress(P: Pointer): Pointer;
		{The address at which P was allocated
		If MemCheck was not running when P was allocated (ie we do not find our magic number), we return $00000000}

	function IsMemCheckActive: boolean;
		{Is MemCheck currently running ?
		ie, is the current memory manager memcheck's ?}

	var
		MemCheckLogFileName: string = 'c:\Temp\MemCheck.log';
			{The file memcheck will log information to}

		DeallocateFreedMemoryWhenBlockBiggerThan: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF} = 0;
			{should blocks be really deallocated when FreeMem is called ? If you want all blocks to be deallocated, set this
			constant to 0. If you want blocks to be never deallocated, set the cstte to MaxInt. When blocks are not deallocated,
			NewsFinder can give information about when the second deallocation occured}

implementation

	uses
		Windows,	{Windows has no finalization, so is OK to use with no care}
		{$IFDEF VER120}Math,{$ENDIF}
		SysUtils;	{Because of this uses, SysUtils must be finalized after MemCheck - Which is necessary anyway because SysUtils calls DoneExceptions in its finalization}

	type
		TKindOfMemory = (MClass, MUser, MReallocedUser);
			{MClass means the block carries an object
			MUser means the block is a buffer of unknown type (in fact we just know this is not an object)
			MReallocedUser means this block was reallocated}

	const
		(**************** MEMCHECK OPTIONS ********************)

		StoredCallStackDepth = 10;
			{Size of the call stack we store when GetMem is called, must be an EVEN number}

		DanglingInterfacesVerified = True;
			{When an object is destroyed, should we fill the interface VMT with a special value which
			will allow tracking of calls to this interface after the object was destroyed}

		WipeOutMemoryOnFreeMem = False;
			{should we fill the memory zone with a char when freeing it ?}
		CharToUseToWipeOut: char = chr($00);

		CheckWipedBlocksOnTermination = True and WipeOutMemoryOnFreeMem and not(DanglingInterfacesVerified);
			{When iterating on the blocks (in OutputAllocatedBlocks), we check for every block which has been deallocated that it is still
			filled with CharToUseToWipeOut.
			Warning: this is VERY time-consuming
			This is meaningful only when the blocks are wiped out on free mem
			This is incompatible with dangling interfaces checking}
		DoNotCheckWipedBlocksBiggerThan = 4000;

		CollectStatsAboutObjectAllocation = False;
			{Every time FreeMem is called for allocationg an object, this will register information about the class instanciated:
			class name, number of instances, allocated space for one instance
			Note: this has to be done on FreeMem because when GetMem is called, the VMT is not installed yet and we can not know
			this is an object}

		KeepMaxMemoryUsage = CollectStatsAboutObjectAllocation;
			{Will report the biggest memory usage during the execution}

		ComputeMemoryUsageStats = False;
			{Outputs the memory usage along the life of the execution. This output can be easily graphed, in excel for example}
		MemoryUsageStatsStep = 5;
			{Meaningful only when ComputeMemoryUsageStats
			When this is set to 5, we collect information for the stats every 5 call to GetMem, unless size is bigger than StatCollectionForce}
		StatCollectionForce = 1000;

		BlocksToShow: array[TKindOfMemory] of Boolean = (true, true, true);
			{eg if BlocksToShow[MClass] is True, the blocks allocated for class instances will be shown}

		CheckHeapStatus = False;
			// Checks that the heap has not been corrupted since last call to the memory manager
			// Warning: VERY time-consuming

		IdentifyObjectFields = False;
		IdentifyFieldsOfObjectsConformantTo: TClass = Tobject;

		MaxLeak = 1000;
			{This option tells to MemCheck not to display more than a certain quantity of leaks, so that the finalization
			phase does not take too long}
		IgnoreMaxLeak = True;
			{Should we display all leaks even if there are more than MaxLeak ?}

		UseDebugInfos = True;
			//Should use the debug informations which are in the executable ?

		(**************** END OF MEMCHECK OPTIONS ********************)

	var
		ShowCallStack: Boolean;
			{When we show an allocated block, should we show the call stack that went to the allocation ? Set to false
			before each block. The usual way to use this is calling Evaluate/Modify just after an EMemoryLeak was raised}

	const
		MaxListSize = MaxInt div 16 - 1;

	type
		PObjectsArray = ^TObjectsArray;
		TObjectsArray = array[0..MaxListSize] of TObject;
		
		PStringsArray = ^TStringsArray;
		TStringsArray = array[0..99999999] of string;
			{Used to simulate string lists}

		PIntegersArray = ^TIntegersArray;
		TIntegersArray = array[0..99999999] of integer;
			{Used to simulate lists of integer}

		PClassesArray = ^TClassesArray;
		TClassesArray = array[0..99999999] of TClass;
			{Used to simulate arrays of TClass}

	var
		TimeStamps: PStringsArray = Nil;
			{Allows associating a string of information with a time stamp}
		TimeStampsCount: integer = 0;
			{Number of time stamps in the array}
		TimeStampsAllocated: integer = 0;
			{Number of positions available in the array}

	const
		DeallocateInstancesConformingTo = False;
		InstancesConformingToForDeallocation: TClass = TObject;
			{used only when BlocksToShow[MClass] is True - eg If InstancesConformingTo = TList, only blocks allocated for instances
			of TList and its heirs will be shown}

		InstancesConformingToForReporting: TClass = TObject;
			{used only when BlocksToShow[MClass] is True - eg If InstancesConformingTo = TList, only blocks allocated for instances
			of TList and its heirs will be shown}

		MaxNbSupportedVMTEntries = 200;
		{Don't change this number, its a Hack! jm}

		Displ: Cardinal = $400000;

	type
		CallStack = Array[0..StoredCallStackDepth] of Pointer;

		PMemoryBlocHeader = ^TMemoryBlocHeader;
		TMemoryBlocHeader = record
			{
			This is the header we put in front of a memory block
			For each memory allocation, we allocate "size requested + header size + footer size" because we keep information inside the memory zone.
			Therefore, the address returned by GetMem is: [the address we get from OldMemoryManager.GetMem] + HeaderSize.

			. DestructionAdress: an identifier telling if the bloc is active or not (when FreeMem is called we do not really free the mem).
			  Nil when the block has not been freed yet; otherwise, contains the address of the caller of the destruction. This will be useful
			  for reporting errors such as "this memory has already been freed, at address XXX".
			. PreceedingBlock: link of the linked list of allocated blocs
			. NextBlock: link of the linked list of allocated blocs
			. KindOfBlock: is the data an object or unknown kind of data (such as a buffer)
			. VMT: the classtype of the object
			. CallerAddress: an array containing the call stack at allocation time
			. AllocatedSize: the size allocated for the user (size requested by the user)
			. MagicNumber: an integer we use to recognize a block which was allocated using our own allocator
			}
				DestructionAdress : Pointer;
				PreceedingBlock: Pointer;
				NextBlock: Pointer;
				KindOfBlock: TKindOfMemory;
				VMT: TClass;
				CallerAddress: CallStack;
				AllocatedSize,
				MagicNumber : {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
			end;

		PMemoryBlockFooter = ^TMemoryBlockFooter;
		TMemoryBlockFooter = {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
			{This is the end-of-bloc marker we use to check that the user did not write beyond the allowed space}

		EMemoryLeak = class(Exception);
		EStackUnwinding = class(EMemoryLeak);
		EBadInstance = class(Exception);
			{This exception is raised when a virtual method is called on an object which has been freed}
		EFreedBlockDamaged = class(Exception);
		EInterfaceFreedInstance = class(Exception);
			{This exception is raised when a method is called on an interface whom object has been freed}

		VMTTable = array[0..MaxNbSupportedVMTEntries] of pointer;
		pVMTTable = ^VMTTable;
		TMyVMT = record
			A: array[0..47] of byte;
			B: VMTTable;
		end;

		ReleasedInstance = Class
			procedure RaiseExcept;
			procedure InterfaceError; stdcall;
			procedure Error; Virtual;
		end;

		TFieldInfo = class
			OwnerClass: TClass;
			FieldIndex: integer;

			constructor Create(const TheOwnerClass: TClass; const TheFieldIndex: integer);
		end;

	const
		EndOfBlock: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF} = $FFFFFFFA;
		Magic: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF} = $FFFFFFFF;
		BlockDescription: array[TKindOfMemory] of String[10] = ('CLASS   ', 'USER    ', 'REALLOC ');

	var
		FreedInstance : PChar;
		BadObjectVMT: TMyVMT;
		BadInterfaceVMT : VMTTable;
		GIndex : Integer;

		LastBlock: PMemoryBlocHeader;

		MemCheckActive: boolean = False;
			{Is MemCheck currently running ?
			ie, is the current memory manager memcheck's ?}
		MemCheckInitialized: Boolean = False;
			{Has InitializeOnce been called ?
			This variable should ONLY be used by InitializeOnce and the finalization}

		{arrays for stats}
		AllocatedObjectsClasses: PClassesArray = Nil;	{classes}
		AllocatedInstances: PIntegersArray = Nil;	{instances counter}
		AllocStatsCount: integer = 0;
		StatsArraysAllocatedPos: integer = 0;
			{This is used to display some statistics about objects allocated. Each time an object is allocated, we look if its
			class name appears in this list. If it does, we increment the counter of class' instances for this class;
			if it does not appear, we had it with a counter set to one.}

		MemoryUsageStats: PIntegersArray = Nil;	{instances counter}
		MemoryUsageStatsCount: integer = 0;
		MemoryUsageStatsAllocatedPos: integer = 0;
		MemoryUsageStatsLoop: integer = -1;

		SevereExceptionsLogFile: Text;
			{This is the log file for exceptions}

		OutOfMemory: EOutOfMemory;
			// Because when we have to raise this, we do not want to have to instanciate it (as there is no memory available)

		HeapCorrupted: Exception;

		NotDestroyedFields: PIntegersArray = Nil;
		NotDestroyedFieldsInfos: PObjectsArray = Nil;
		NotDestroyedFieldsCount: integer = 0;
		NotDestroyedFieldsAllocatedSpace: integer = 0;

		LastHeapStatus: THeapStatus;

		MaxMemoryUsage: Integer = 0;
			// see KeepMaxMemoryUsage

	type
		TIntegerBinaryTree = class
			protected
				fValue: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
				fBigger: TIntegerBinaryTree;
				fSmaller: TIntegerBinaryTree;

				class function StoredValue(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
				constructor _Create(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
				function _Has(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): Boolean;
				procedure _Add(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
				procedure _Remove(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});

			public
				function Has(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): Boolean;
				procedure Add(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
				procedure Remove(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});

				property Value: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF} read fValue;
			end;

		PCardinal = ^{$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};

	var
		CurrentlyAllocatedBlocksTree: TIntegerBinaryTree;

	type
		{$IFNDEF VER120}
		TObjectList = class
			//A list for MemCheck - Remember we can not use TList because we don't want to use classes
			protected
				fData: PObjectsArray;
				fCount: Integer;
				fCapacity: Integer;

				function Get(const Index: Integer): TObject;
				procedure Grow;
				procedure SetCapacity(const C: Integer);
				procedure SetCount(const C: Integer);

			public
				constructor Create;
				destructor Destroy; override;
				procedure Add(const O: TObject);
				property Items[const Index: Integer]: TObject read Get; default;
				property Count: Integer read fCount;
				procedure Clear;
				property Capacity: Integer read fCapacity;
			end;
		{$ENDIF}

		TAddressToLine = class
			public
				Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
				Line: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};

				constructor Create(const AAddress, ALine: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
			end;

		PAddressesArray = ^TAddressesArray;
		TAddressesArray = array[0..MaxInt div 16 - 1] of TAddressToLine;

		TUnitDebugInfos = class
			public
				Name: string;
				Addresses: {$IFNDEF VER120}PAddressesArray{$ELSE}array of TAddressToLine{$ENDIF};
				{$IFNDEF VER120}AddressesCount: Cardinal;{$ENDIF}

				constructor Create(const AName: string; const NbLines: Cardinal);
				{$IFNDEF VER120}
				destructor Destroy; override;
				{$ENDIF}

				function LineWhichContainsAddress(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): string;
			end;

		TRoutineDebugInfos = class
			public
				Name: string;
				StartAddress: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
				EndAddress: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};

				constructor Create(const AName: string; const AStartAddress: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}; const ALength: Cardinal);
			end;

	var
		{$IFNDEF VER120}
		Routines: TObjectList;	{of TRoutineDebugInfos}
		Units: TObjectList;	{of TUnitDebugInfos}
		{$ELSE}
		Routines: array of TRoutineDebugInfos;
		RoutinesCount: integer;
		Units: array of TUnitDebugInfos;
		UnitsCount: integer;
		{$ENDIF}

	procedure UpdateLastHeapStatus;
		begin
			LastHeapStatus:= GetHeapStatus;
		end;

	function HeapStatusesDifferent(const Old, New: THeapStatus): boolean;
		begin
			Result:=
				(Old.TotalAddrSpace 	<> New.TotalAddrSpace) or
				(Old.TotalUncommitted <> New.TotalUncommitted) or
				(Old.TotalCommitted <> New.TotalCommitted) or
				(Old.TotalAllocated <> New.TotalAllocated) or
				(Old.TotalFree <> New.TotalFree) or
				(Old.FreeSmall <> New.FreeSmall) or
				(Old.FreeBig <> New.FreeBig) or
				(Old.Unused <> New.Unused) or
				(Old.Overhead <> New.Overhead) or
				(Old.HeapErrorCode <> New.HeapErrorCode) or
				(New.TotalUncommitted + New.TotalCommitted <> New.TotalAddrSpace) or
				(New.Unused + New.FreeBig + New.FreeSmall <> New.TotalFree)
		end;

	class function TIntegerBinaryTree.StoredValue(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
		begin
			Result:= Address shl 16;
			Result:= Result or (Address shr 16);
			Result:= Result xor $AAAAAAAA;
		end;
                              
	constructor TIntegerBinaryTree._Create(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
		begin
        	// Just for optimization, I do not call inherited Create 

			fValue:= Address
		end;

	function TIntegerBinaryTree.Has(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): Boolean;
		begin
			Result:= _Has(StoredValue(Address));
		end;

	procedure TIntegerBinaryTree.Add(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
		begin
			_Add(StoredValue(Address));
		end;

	procedure TIntegerBinaryTree.Remove(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
		begin
			_Remove(StoredValue(Address));
		end;

	function TIntegerBinaryTree._Has(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): Boolean;
		begin
			if fValue = Address then
				Result:= True
			else
				if (Address > fValue) and (fBigger <> Nil) then
					Result:= fBigger._Has(Address)
				else
					if (Address < fValue) and (fSmaller <> Nil) then
						Result:= fSmaller._Has(Address)
					else
						Result:= False
		end;

	procedure TIntegerBinaryTree._Add(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
		begin
			Assert(Address <> fValue, 'TIntegerBinaryTree._Add: already in !');

			if (Address > fValue) then
				begin
					if fBigger <> Nil then
						fBigger._Add(Address)
					else
						fBigger:= TIntegerBinaryTree._Create(Address)
				end
			else
				begin
					if fSmaller <> Nil then
						fSmaller._Add(Address)
					else
						fSmaller:= TIntegerBinaryTree._Create(Address)
				end
		end;

	procedure TIntegerBinaryTree._Remove(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
		var
			Owner, Node: TIntegerBinaryTree;
			NodeIsOwnersBigger: Boolean;
			Middle, MiddleOwner: TIntegerBinaryTree;
		begin
			Owner:= Nil;
			Node:= CurrentlyAllocatedBlocksTree;

			while (Node <> Nil) and (Node.fValue <> Address) do
				begin
					Owner:= Node;

					if Address > Node.Value then
						Node:= Node.fBigger
					else
						Node:= Node.fSmaller
				end;

			Assert(Node <> Nil, 'TIntegerBinaryTree._Remove: not in');

			NodeIsOwnersBigger:= Node = Owner.fBigger;

			if Node.fBigger = Nil then
				begin
					if NodeIsOwnersBigger then
						Owner.fBigger:= Node.fSmaller
					else
						Owner.fSmaller:= Node.fSmaller;
				end
			else
				if Node.fSmaller = Nil then
					begin
						if NodeIsOwnersBigger then
							Owner.fBigger:= Node.fBigger
						else
							Owner.fSmaller:= Node.fBigger;
					end
				else
					begin
						Middle:= Node.fSmaller;
						MiddleOwner:= Node;

						while Middle.fBigger <> Nil do
							begin
								MiddleOwner:= Middle;
								Middle:= Middle.fBigger;
							end;

						if Middle = Node.fSmaller then
							begin
								if NodeIsOwnersBigger then
									Owner.fBigger:= Middle
								else
									Owner.fSmaller:= Middle;

								Middle.fBigger:= Node.fBigger
							end
						else
							begin
								MiddleOwner.fBigger:= Middle.fSmaller;

								Middle.fSmaller:= Node.fSmaller;
								Middle.fBigger:= Node.fBigger;

								if NodeIsOwnersBigger then
									Owner.fBigger:= Middle
								else
									Owner.fSmaller:= Middle
							end;
					end;

			Node.Destroy;
		end;

	function Caller: Pointer;
		asm
			mov 	eax,[ebp];
			mov 	eax,[eax+4];
			sub     eax,5
		end;

	function EBP8Caller: Pointer;
		asm
			mov     eax,[EBP+8];
			sub     eax, 5
		end;

	constructor TFieldInfo.Create(const TheOwnerClass: TClass; const TheFieldIndex: integer);
		begin
			inherited Create;

			OwnerClass:= TheOwnerClass;
			FieldIndex:= TheFieldIndex;
		end;

	procedure ReleasedInstance.RaiseExcept;
		var
			t: TMemoryBlocHeader;
			i: integer;
		begin
			t:= PMemoryBlocHeader((PChar(Self) - SizeOf(TMemoryBlocHeader)))^;

			try
				if GIndex = MaxNbSupportedVMTEntries then
					Raise EBadInstance.Create('Call ' + T.VMT.ClassName + '.Destroy on a FREED instance') at PChar(pVMTTable(PChar(T.VMT)-4)^[0]) + 1
				else
					Raise EBadInstance.Create('Call ' + T.VMT.ClassName + '.' +
						'<'+IntToHex(Cardinal(pVMTTable(T.VMT)^[MaxNbSupportedVMTEntries-1-GIndex])-Displ, 8)+'>'
						+' on a FREED instance') at PChar(pVMTTable(T.VMT)^[MaxNbSupportedVMTEntries-1-GIndex]) + 1;
			except
				on EBadInstance do ;
			end;

			if ShowCallStack then
				for i:= 1 to StoredCallStackDepth do
					if Integer(T.CallerAddress[i])>0 then
						try
							raise EStackUnwinding.Create('Unwinding level '+chr(ord('0')+i)) at T.CallerAddress[i]
						except
							on EStackUnwinding do;
						end;

			ShowCallStack:= False;
		end;

	function InterfaceErrorCaller: Pointer;
		{Returns EBP + 16, which is OK only for InterfaceError !
		It would be nice to make this routine local to InterfaceError, but I do not know hot to
		implement it in this case - VM}
		asm
			mov     eax,[EBP+16];
			sub     eax, 5
		end;

	procedure ReleasedInstance.InterfaceError;
		begin
			try
				Raise EInterfaceFreedInstance.Create('Calling an interface method on an freed Pascal instance') at InterfaceErrorCaller
			except
				on EInterfaceFreedInstance do
					;
			end;
		end;

	procedure ReleasedInstance.Error;
		{Don't change this, its a Hack! jm}
		asm
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);Inc(GIndex);
		JMP ReleasedInstance.RaiseExcept;
		end;

	function MemoryBlockDump(Block: PMemoryBlocHeader): string;
		const
			MaxDump = 80;
		var
			i,
			count: integer;
			s: string[MaxDump];
		begin
			count:= Block.AllocatedSize;

			if count > MaxDump then
				Count:= MaxDump;

			Byte(s[0]):= count;
			move((PChar(Block) + SizeOf(TMemoryBlocHeader))^, s[1], Count);

			for i:= 1 to Length(s) do
				if s[i] = #0 then s[i]:= '.' else
					if s[i] < ' ' then
						s[i]:= '?';

			Result:= '  Dump: [' + s + ']';
		end;

	function EBP8_10: Pointer;
		asm
			mov     eax,[EBP+8]
			sub     eax, 9
		end;

	function EBP36_12: Pointer;
		asm
			mov     eax,[EBP+36]
			sub     eax, 12
		end;

	function EBP12Caller: Pointer;
		asm
			mov     eax,[EBP+12];
			sub     eax, 5
		end;

	function EBP56Caller: Pointer;
		asm
			mov     eax,[EBP+56];
			sub     eax, 5
		end;

	procedure FillCallStack(var St : CallStack; Deeply : Boolean);
		var
			i : integer;
			_EBP : Integer;
			_ESP : Integer;
		begin
			asm
				mov  eax, ESP
				mov  _ESP,eax
				mov  eax, EBP
				mov  _EBP, eax
			end;
			if Deeply then
				begin
					_ESP:= _EBP;
					_EBP:= PInteger(_EBP)^;
				 end;
			FillChar(St, SizeOf(St), 0);
			if (_EBP<_ESP) or (_EBP-_ESP>30000) then Exit;
			for i:= 0 to StoredCallStackDepth do
				begin
					_ESP:= _EBP;
					_EBP:= PInteger(_EBP)^;
					if (_EBP<_ESP) or (_EBP-_ESP>30000) then Exit;
					St[i]:= Pointer(PInteger(_EBP+4)^-4);
				end;
		end;

	procedure CollectNewInstanceOfClassForStats(const TheClass: TClass);
		var
			i: integer;
		begin
			i:= 0;
			while (i < AllocStatsCount) and (AllocatedObjectsClasses[i] <> TheClass) do
				i:= i + 1;

			if i = AllocStatsCount then
				begin
					if AllocStatsCount = StatsArraysAllocatedPos then
						begin
							if StatsArraysAllocatedPos = 0 then
								StatsArraysAllocatedPos:= 10;
							StatsArraysAllocatedPos:= StatsArraysAllocatedPos * 2;
							UnMemChk;
							ReallocMem(AllocatedObjectsClasses, StatsArraysAllocatedPos * sizeof(WideString));
							ZeroMemory(pointer(integer(AllocatedObjectsClasses) + AllocStatsCount * sizeof(WideString)), (StatsArraysAllocatedPos - AllocStatsCount) * SizeOf(WideString));
							ReallocMem(AllocatedInstances, StatsArraysAllocatedPos * sizeof(Integer));
							MemChk;
						end;

					AllocatedObjectsClasses[AllocStatsCount]:= TheClass;
					AllocatedInstances[AllocStatsCount]:= 1;
					AllocStatsCount:= AllocStatsCount + 1;
				end
			else
				AllocatedInstances[i]:= AllocatedInstances[i] + 1;
		end;

	function LeakTrackingGetMem(Size: Integer): Pointer;
		begin
			if (EBP8_10 = PChar(@TObject.NewInstance)) {comming from TObject.NewInstance ?} then
				begin
					Result:= SysGetMem(Size + (SizeOf(TMemoryBlocHeader)));
					if Result = Nil then
						Raise OutOfMemory;
					PMemoryBlocHeader(Result).KindOfBlock:= MClass;
					if EBP36_12 = PChar(@TObject.Create) then
						begin
							if StoredCallStackDepth > 0 then
								FillCallStack(PMemoryBlocHeader(Result).CallerAddress, false);
							PMemoryBlocHeader(Result).CallerAddress[0]:= EBP56Caller;
						  end
					else
						begin
							if StoredCallStackDepth > 0 then
								FillCallStack(PMemoryBlocHeader(Result).CallerAddress, true);
							PMemoryBlocHeader(Result).CallerAddress[0]:= Caller;
						  end;
				end
			else
				if Integer(EBP8_10) < Integer(@FPower10) then
					{a hack to see that this is a string allocation
					This is just a heuristics - worked ok so far
					We do not see any good reason for tracking memory allocation for strings
					However, you can do it by just out-commenting this part of the code}
					begin
						Result:= SysGetMem(Size);
						if Result = Nil then
							Raise OutOfMemory;
						Exit;
					end
				else
					begin	{Neither an object nor a string, this is a MUser}
						Result:= SysGetMem(Size + (SizeOf(TMemoryBlocHeader) + SizeOf(TMemoryBlockFooter)));
						if Result = Nil then
							Raise OutOfMemory;
						PMemoryBlocHeader(Result).KindOfBlock:= MUser;
						if StoredCallStackDepth > 0 then
							FillCallStack(PMemoryBlocHeader(Result).CallerAddress, false);
						PMemoryBlocHeader(Result).CallerAddress[0]:= EBP8Caller;
						PMemoryBlockFooter(PChar(Result) + SizeOf(TMemoryBlocHeader) + Size)^:= EndOfBlock;
					end;

			PMemoryBlocHeader(Result).PreceedingBlock:= LastBlock;
			PMemoryBlocHeader(Result).NextBlock:= Nil;
			if LastBlock <> Nil then
				LastBlock.NextBlock:= Result;
			LastBlock:= Result;
			LastBlock.DestructionAdress:= Nil;
			LastBlock.AllocatedSize:= Size;
			LastBlock.MagicNumber:= Magic;

			if IdentifyObjectFields then
				begin
					UnMemChk;
					CurrentlyAllocatedBlocksTree.Add(integer(LastBlock));
					MemChk;
				end;

			Inc(integer(Result), SizeOf(TMemoryBlocHeader));

			if ComputeMemoryUsageStats then
				begin
					MemoryUsageStatsLoop:= MemoryUsageStatsLoop + 1;
					if MemoryUsageStatsLoop = MemoryUsageStatsStep then
						MemoryUsageStatsLoop:= 0;

					if (MemoryUsageStatsLoop = 0) or (Size > StatCollectionForce) then
						begin
							if MemoryUsageStatsCount = MemoryUsageStatsAllocatedPos then
								begin
									if MemoryUsageStatsAllocatedPos = 0 then
										MemoryUsageStatsAllocatedPos:= 10;
									MemoryUsageStatsAllocatedPos:= MemoryUsageStatsAllocatedPos * 2;
									UnMemChk;
									ReallocMem(MemoryUsageStats, MemoryUsageStatsAllocatedPos * sizeof(Integer));
									MemChk;
								end;

							MemoryUsageStats[MemoryUsageStatsCount]:= AllocMemSize;
							MemoryUsageStatsCount:= MemoryUsageStatsCount + 1;
						end;
				end;

			if KeepMaxMemoryUsage and (AllocMemSize > MaxMemoryUsage) then
				MaxMemoryUsage:= AllocMemSize;
		end;

	function HeapCheckingGetMem(Size: Integer): Pointer;
		begin
			if HeapStatusesDifferent(LastHeapStatus, GetHeapStatus) then
				Raise HeapCorrupted;

			Result:= SysGetMem(Size);

			UpdateLastHeapStatus;
		end;

	function MemoryBlockCorrupted(P: Pointer): Boolean;
		var
			Block: PMemoryBlocHeader;
		begin
			if PCardinal(PChar(P)-4)^ = Magic then
				begin
					Block:= PMemoryBlocHeader(PChar(P) - SizeOf(TMemoryBlocHeader));
					Result:= (Block.DestructionAdress <> Nil);
					Result:= Result or (PMemoryBlockFooter(PChar(P) + Block.AllocatedSize)^ <> EndOfBlock)
				end
			else
				Result:= False
		end;

	procedure ReplaceInterfacesWithBadInterface(AClass : TClass; Instance: Pointer);
			{copied and modified from System.Pas: replaces all INTERFACES in Pascal Objects
			with a reference to our dummy INTERFACE VMT}
		asm
				PUSH    EBX
				PUSH    ESI
				PUSH    EDI
				MOV     EBX,EAX
				MOV     EAX,EDX
				MOV     EDX,ESP
		@@0:    MOV     ECX,[EBX].vmtIntfTable
				TEST    ECX,ECX
				JE      @@1
				PUSH    ECX
		@@1:    MOV     EBX,[EBX].vmtParent
				TEST    EBX,EBX
				JE      @@2
				MOV     EBX,[EBX]
				JMP     @@0
		@@2:    CMP     ESP,EDX
				JE      @@5
		@@3:    POP     EBX
				MOV     ECX,[EBX].TInterfaceTable.EntryCount
				ADD     EBX,4
		@@4:    LEA     ESI, BadInterfaceVMT // mettre dans ESI l'adresse du début de MyInterfaceVMT: correct ?????
				MOV     EDI,[EBX].TInterfaceEntry.IOffset
				MOV     [EAX+EDI],ESI
				ADD     EBX,TYPE TInterfaceEntry
				DEC     ECX
				JNE     @@4
				CMP     ESP,EDX
				JNE     @@3
		@@5:    POP     EDI
				POP     ESI
				POP     EBX
		end;

	function BlockAllocationAddress(P: Pointer): Pointer;
		var
			Block: PMemoryBlocHeader;
		begin
			Block:= PMemoryBlocHeader(PChar(P) - SizeOf(TMemoryBlocHeader));

			Result:= Pointer(PChar(Block.CallerAddress[0]) - Displ)
		end;

	function FindMem(Base, ToFind : pointer; Nb : integer) : integer;
			// Base = instance, Nb = nombre de bloc (HORS VMT!)
		asm
			// eax=base; edx=Tofind; ecx=Nb
			@loop:
			cmp [eax+ecx*4], edx
			je @found
			dec ecx
			jne  @loop

			@found:
			mov eax,ecx
		end;

	procedure AddFieldInfo(const FieldAddress: Pointer; const OwnerClass: TClass; const FieldPos: integer);
		begin
			UnMemChk;

			if NotDestroyedFieldsCount = NotDestroyedFieldsAllocatedSpace then
				begin
					if NotDestroyedFieldsAllocatedSpace = 0 then
						NotDestroyedFieldsAllocatedSpace:= 10;
					NotDestroyedFieldsAllocatedSpace:= NotDestroyedFieldsAllocatedSpace * 2;
					ReallocMem(NotDestroyedFields, NotDestroyedFieldsAllocatedSpace * sizeof(integer));
					ReallocMem(NotDestroyedFieldsInfos, NotDestroyedFieldsAllocatedSpace * sizeof(integer));
				end;

			NotDestroyedFields[NotDestroyedFieldsCount]:= integer(FieldAddress);
			NotDestroyedFieldsInfos[NotDestroyedFieldsCount]:= TFieldInfo.Create(OwnerClass, FieldPos);
			NotDestroyedFieldsCount:= NotDestroyedFieldsCount + 1;

			MemChk;
		end;

	function LeakTrackingFreeMem(P: Pointer): Integer;
		var
			Block: PMemoryBlocHeader;
			i: integer;
		begin
			if PCardinal(PChar(P)-4)^ = Magic then
				{we recognize a block we marked}
				begin
					Block:= PMemoryBlocHeader(PChar(P) - SizeOf(TMemoryBlocHeader));

					if CollectStatsAboutObjectAllocation and (Block.KindOfBlock = MClass) then
						CollectNewInstanceOfClassForStats(TObject(P).ClassType);

					if IdentifyObjectFields then
						begin
							if (Block.KindOfBlock = MClass) and (TObject(P).InheritsFrom(IdentifyFieldsOfObjectsConformantTo)) then
								for i:= 1 to (Block.AllocatedSize div 4) - 1 do
									If (PInteger(PChar(P) + i * 4)^ > -MaxInt + SizeOf(TMemoryBlocHeader)) and CurrentlyAllocatedBlocksTree.Has(PInteger(PChar(P) + i * 4)^ - SizeOf(TMemoryBlocHeader)) then
										AddFieldInfo(Pointer(PInteger(PChar(P) + i * 4)^ - SizeOf(TMemoryBlocHeader)), TObject(P).ClassType, i);

							UnMemChk;
							if Block.DestructionAdress = Nil then
								begin
									Assert(CurrentlyAllocatedBlocksTree.Has(integer(Block)), 'freemem: block not among allocated ones');
									CurrentlyAllocatedBlocksTree.Remove(integer(Block));
								end;
							MemChk;
						end;

					if (Block.AllocatedSize > DeallocateFreedMemoryWhenBlockBiggerThan) or
						(DeallocateInstancesConformingTo and (Block.KindOfBlock = MClass) and (TObject(P) is InstancesConformingToForDeallocation)) then
						{we really deallocate the block}
						begin
							if Block.NextBlock <> Nil then
								PMemoryBlocHeader(Block.NextBlock).PreceedingBlock:= Block.PreceedingBlock;
							if Block.PreceedingBlock <> Nil then
								PMemoryBlocHeader(Block.PreceedingBlock).NextBlock:= Block.NextBlock;
							if LastBlock = Block then
								LastBlock:= Block.PreceedingBlock;

							SysFreeMem(Block);
						end
					else
						if Block.DestructionAdress <> Nil then
							begin
								try
									raise EMemoryLeak.Create('second release of block attempt, allocated') at Block.CallerAddress[0];
								except
									on EMemoryLeak do;
								end;

								if ShowCallStack then
									for i:= 1 to StoredCallStackDepth do
										if Integer(Block.CallerAddress[i])>0 then
											try
												raise EStackUnwinding.Create('Unwinding level '+chr(ord('0')+i)) at Block.CallerAddress[i]
											except
												on EStackUnwinding do;
											end;

								ShowCallStack:= False;
							end
						else
							begin
								if (Block.KindOfBlock<>MClass) and MemoryBlockCorrupted(P) then
									begin
										try
											raise EMemoryLeak.Create('memory damaged beyond block allocated space, allocated at ' + IntToHex(Cardinal(BlockAllocationAddress(P)),8));
										except
											on EMemoryLeak do;
										end;
									end;

								Block.DestructionAdress:= (*Caller*) Pointer($FFFFFFFF);

								FillCallStack(Block.CallerAddress, false);

								if WipeOutMemoryOnFreeMem then
									if Block.KindOfBlock=MClass then
										begin
											Block.VMT:= TObject(P).ClassType;
											FillChar((PChar(P) + 4)^, Block.AllocatedSize - 4, CharToUseToWipeOut);
											PInteger(P)^:= Integer(FreedInstance);
											if DanglingInterfacesVerified then
												ReplaceInterfacesWithBadInterface(Block.VMT, TObject(P))
										end
									else
										FillChar(P^, Block.AllocatedSize, CharToUseToWipeOut);
							end;

					Result:= 0;
				end
			else
				Result:= SysFreeMem(P);
		end;

	function HeapCheckingFreeMem(P: Pointer): Integer;
		begin
			if HeapStatusesDifferent(LastHeapStatus, GetHeapStatus) then
				Raise HeapCorrupted;

			Result:= SysFreeMem(P);

			UpdateLastHeapStatus;
		end;

	function LeakTrackingReallocMem(P: Pointer; Size: Integer): Pointer;
		begin
			if PCardinal(PChar(P)-4)^ = Magic then
				begin
					GetMem(Result, Size);
					if StoredCallStackDepth > 0 then
						FillCallStack(LastBlock.CallerAddress, false);
					LastBlock.CallerAddress[0]:= EBP12Caller; {12 because _ReallocMem push one more argument}
					LastBlock.KindOfBlock:= MReallocedUser;

					if {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(Size) > PMemoryBlocHeader(PChar(P) - SizeOf(TMemoryBlocHeader)).AllocatedSize then
						Move(P^, Result^, PMemoryBlocHeader(PChar(P) - SizeOf(TMemoryBlocHeader)).AllocatedSize)
					else
						Move(P^, Result^, Size);
						
					LeakTrackingFreeMem(P);
				end
			else
				Result:= SysReallocMem(P, Size);
		end;

	function HeapCheckingReallocMem(P: Pointer; Size: Integer): Pointer;
		begin
			if HeapStatusesDifferent(LastHeapStatus, GetHeapStatus) then
				Raise HeapCorrupted;

			Result:= SysReallocMem(P, Size);

			UpdateLastHeapStatus;
		end;

	procedure UnMemChk;
		const
			DefaultMemoryManager: TMemoryManager = (
				GetMem: SysGetMem;
				FreeMem: LeakTrackingFreeMem;
				ReallocMem: LeakTrackingReallocMem;
			   );
		begin
			SetMemoryManager(DefaultMemoryManager);
			MemCheckActive:= False;
		end;

	function IsMemFilledWithChar(P : Pointer; N : Integer; C : Char) : boolean;
		{is the memory at P made of C on N bytes ?}
		asm
		{     ->EAX     Pointer to memory }
		{       EDX     count   }
		{       CL      value   }
		@loop:
		cmp   [eax+edx-1],cl
		jne   @diff
		dec   edx
		jne   @loop
		mov   eax,1
		ret
		@diff:
		xor   eax,eax
		end;

	procedure GoThroughAllocatedBlocks;
		{traverses the allocated blocks list and for each one, raises exceptions showing the memory leaks}
		var
			Block: PMemoryBlocHeader;
			i: integer;
			S : ShortString;
		begin
			UnMemChk;

			{defaults for blocks displaying}
			Block:= LastBlock;

			ShowCallStack:= False;	{for first block}

			while Block <> Nil do
				begin
					if BlocksToShow[Block.KindOfBlock] then
						begin
							if Block.DestructionAdress = Nil then
								{this is a leak}
								begin
									if Block.KindOfBlock = MClass then
										S:= TObject(PChar(Block)+SizeOf(TMemoryBlocHeader)).ClassName
									else
										S:= BlockDescription[Block.KindOfBlock];

									if (BlocksToShow[Block.KindOfBlock]) and ((Block.KindOfBlock <> MClass) or (TObject(PChar(Block) + SizeOf(TMemoryBlocHeader)) is InstancesConformingToForReporting)) then
										try
											raise EMemoryLeak.Create(S+' allocated at '+ IntToHex(Cardinal(Block.CallerAddress[0])-Displ, 8)) at Block.CallerAddress[0]
										except
											on EMemoryLeak do;
										end;

									if ShowCallStack then
										for i:= 1 to StoredCallStackDepth do
											if Integer(Block.CallerAddress[i])>0 then
												try
													raise EStackUnwinding.Create(S+' unwinding level '+chr(ord('0')+i)) at Block.CallerAddress[i]
												except
													on EStackUnwinding do;
												end;

									ShowCallStack:= False;
								end	{Block.DestructionAdress = Nil}
							else
								{this is not a leak}
								if CheckWipedBlocksOnTermination and (Block.AllocatedSize > 5) and (Block.AllocatedSize <= DoNotCheckWipedBlocksBiggerThan) and (not IsMemFilledWithChar(pchar(Block) + SizeOf(TMemoryBlocHeader) + 4, Block.AllocatedSize - 5, CharToUseToWipeOut)) then
									begin
										try
											raise EFreedBlockDamaged.Create('Destroyed block damaged - Block allocated at ' + IntToHex(Cardinal(Block.CallerAddress[0])-Displ,8) + ' - destroyed at ' + IntToHex(Cardinal(Block.DestructionAdress),8)) at Block.CallerAddress[0]
										except
											on EFreedBlockDamaged do;
										end;
									end;
						end;

					Block:= Block.PreceedingBlock;
				end;
		end;

	procedure dummy; forward;

(*** The hack below arranges for MemCheck's finalization to be called after all others ***)		
	type
  JmpInstruction =
  packed record
    opCode:   Byte;
	distance: Longint;
  end;
  
  TExcDescEntry =
  record
    vTable:  Pointer;
    handler: Pointer;
  end;
	  PExcDesc = ^TExcDesc;
	  TExcDesc =
	  packed record
		jmp: JmpInstruction;
		case Integer of
		0:      (instructions: array [0..0] of Byte);
		1{...}: (cnt: Integer; excTab: array [0..0{cnt-1}] of TExcDescEntry);
	  end;

	  PExcFrame = ^TExcFrame;
	  TExcFrame =
	  record
		next: PExcFrame;
		desc: PExcDesc;
		hEBP: Pointer;
		case Integer of
		0:  ( );
		1:  ( ConstructedObject: Pointer );
		2:  ( SelfOfMethod: Pointer );
	  end;

	  PInitContext = ^TInitContext;
	  TInitContext = record
		OuterContext:   PInitContext;     { saved InitContext   }
		ExcFrame:       PExcFrame;        { bottom exc handler  }
		InitTable:      PackageInfo;      { unit init info      }
		InitCount:      Integer;          { how far we got      }
		Module:         PLibModule;       { ptr to module desc  }
		DLLSaveEBP:     Pointer;          { saved regs for DLLs }
		DLLSaveEBX:     Pointer;          { saved regs for DLLs }
		DLLSaveESI:     Pointer;          { saved regs for DLLs }
		DLLSaveEDI:     Pointer;          { saved regs for DLLs }
		DLLInitState:   Byte;
		ExitProcessTLS: procedure;        { Shutdown for TLS    }
	  end;

	procedure ChangeFinalizationsOrder;
		{we are going to change the order in which Finalizations will occur
		- SysInit is always finalized last (#0 in the list), and we can not change that (we can not recompile System, which has a "uses SysInit")
		- System is always finalized just before (#1), and we can not change that
		- SysUtils has to be finalized after MemCheck (#2), because SysUtils has "DoneExceptions" in its finalization, and this prevents GoThroughAllocatedBlocks from working OK
		- MemCheck will be next (#3) - You could say MemCheck has uses FileUtil & Windows, but they have neither finalization nor initialization
		}
		const
			NewIndexOfSysutilsFinalization = 2;
			NewIndexOfMemcheckFinalization = NewIndexOfSysutilsFinalization + 1;
		var
			InitContext: PInitContext;
			Table: PUnitEntryTable;
			i: integer;
			MemCheckFinalizationIndex, SysUtilsFinalizationIndex: integer;
			BytesRead: DWord;
			DistToMemCheckFinalization: {$IFDEF VER120}int64{$ELSE}integer{$Endif};
			h: THandle;
			MemCheckUnitEntry, SysUtilsUnitEntry: Pointer;
			CurrentEntry: pointer;
		begin
			InitContext:= PInitContext(PChar(@AllocMemSize)+31*4);
			Table:= InitContext.InitTable^.UnitInfo;
			MemCheckFinalizationIndex:= -1;
			SysUtilsFinalizationIndex:= -1;

			{seek memcheck's finalization}
			DistToMemCheckFinalization:= MaxInt;
			for i:= InitContext.InitCount - 1 downto 0 do
				if ({$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@Table^[i].Finit) > {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@dummy)) and ({$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@Table^[i].Finit) - {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@dummy) < DistToMemCheckFinalization) then
					begin
						MemCheckFinalizationIndex:= i;
						DistToMemCheckFinalization:= abs(integer(@Table^[i].Finit) - integer(@dummy));
					end;

			for i:= 0 to InitContext.InitCount - 1 do
				{$IFDEF VER120}
				if ({$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@Table^[i].Finit) > {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@SysUtils.TMultiReadExclusiveWriteSynchronizer.EndRead)) and ({$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@Table^[i].Finit) - {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@SysUtils.TMultiReadExclusiveWriteSynchronizer.EndRead) > 3130) and ({$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@Table^[i].Finit) - {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}(@SysUtils.TMultiReadExclusiveWriteSynchronizer.EndRead) < 3140) then
				{$ELSE}
				if (integer(@Table^[i].Finit) - integer(@SysUtils.RPR) > 2490) and (integer(@Table^[i].Finit) - integer(@SysUtils.RPR) < 2509) then
				{$ENDIF}
					begin
						Assert(SysUtilsFinalizationIndex = -1, 'MemCheck: Finalization of unit SysUtils found twice');
						SysUtilsFinalizationIndex:= i;
					end;

			Assert(SysUtilsFinalizationIndex <> -1, 'MemCheck: Finalization of unit SysUtils not found');

			GetMem(CurrentEntry, 8);

			h:= getcurrentprocessid;
			h:= openprocess(PROCESS_ALL_ACCESS, true, h);

			{bring SysUtils to its new pos}
			GetMem(SysUtilsUnitEntry, 8);
			ReadProcessMemory(h, pointer(pchar(Table) + SysUtilsFinalizationIndex * sizeof(PackageUnitEntry)), SysUtilsUnitEntry, 8, BytesRead);
				{for an obscure reason, we are not able to write into "table" directly - Using WriteProcessMemory works}
			for i:= SysUtilsFinalizationIndex - 1 downto NewIndexOfSysUtilsFinalization do
				begin
					ReadProcessMemory(h, pointer(pchar(Table) + i * sizeof(PackageUnitEntry)), CurrentEntry, 8, BytesRead);
					WriteProcessMemory(h, pointer(pchar(Table) + (i + 1) * sizeof(PackageUnitEntry)), CurrentEntry, 8, BytesRead);
				end;
			WriteProcessMemory(h, pointer(pchar(Table) + NewIndexOfSysUtilsFinalization * sizeof(PackageUnitEntry)), SysUtilsUnitEntry, 8, BytesRead);
			FreeMem(SysUtilsUnitEntry);

			{bring MemCheck to its new pos}
			GetMem(MemCheckUnitEntry, 8);
			ReadProcessMemory(h, pointer(pchar(Table) + MemCheckFinalizationIndex * sizeof(PackageUnitEntry)), MemCheckUnitEntry, 8, BytesRead);
			for i:= MemCheckFinalizationIndex - 1 downto NewIndexOfMemcheckFinalization do
				begin
					ReadProcessMemory(h, pointer(pchar(Table) + i * sizeof(PackageUnitEntry)), CurrentEntry, 8, BytesRead);
					WriteProcessMemory(h, pointer(pchar(Table) + (i + 1) * sizeof(PackageUnitEntry)), CurrentEntry, 8, BytesRead);
				end;
			WriteProcessMemory(h, pointer(pchar(Table) + NewIndexOfMemcheckFinalization * sizeof(PackageUnitEntry)), MemCheckUnitEntry, 8, BytesRead);
			FreeMem(MemCheckUnitEntry);
		end;

	function UnitWhichContainsAddress(const Address: Cardinal): TUnitDebugInfos;
		var
			Start, Finish, Pivot: integer;
		begin
			Start:= 0;
			Finish:= {$IFNDEF VER120}Units.Count{$ELSE}UnitsCount{$ENDIF} - 1;
			Result:= Nil;

			while Start <= Finish do
				begin
					Pivot:= Start + (Finish - Start) div 2;

					if TUnitDebugInfos(Units[Pivot]).Addresses[0].Address > Address then
						Finish:= Pivot - 1
					else
						{$IFNDEF VER120}
						if TUnitDebugInfos(Units[Pivot]).Addresses[TUnitDebugInfos(Units[Pivot]).AddressesCount - 1].Address < Address then
						{$ELSE}
						if TUnitDebugInfos(Units[Pivot]).Addresses[Length(TUnitDebugInfos(Units[Pivot]).Addresses) - 1].Address < Address then
						{$ENDIF}
							Start:= Pivot + 1
						else
							begin
								{$IFNDEF VER120}
								Result:= TUnitDebugInfos(Units[Pivot]);
								{$ELSE}
								Result:= Units[Pivot];
								{$ENDIF}
								Start:= Finish + 1;
							end;
				end;
		end;

	function RoutineWhichContainsAddress(const Address: Cardinal): string;
		var
			Start, Finish, Pivot: integer;
		begin
			Start:= 0;
			Finish:= {$IFNDEF VER120}Routines.Count{$ELSE}RoutinesCount{$ENDIF} - 1;
			Result:= '';

			while Start <= Finish do
				begin
					Pivot:= Start + (Finish - Start) div 2;

					if TRoutineDebugInfos(Routines[Pivot]).StartAddress > Address then
						Finish:= Pivot - 1
					else
						if TRoutineDebugInfos(Routines[Pivot]).EndAddress < Address then
							Start:= Pivot + 1
						else
							begin
								Result:= ' Routine ' + TRoutineDebugInfos(Routines[Pivot]).Name;
								Start:= Finish + 1;
							end;
				end;
		end;
		
	function TextualDebugInfoForAddress(const Address: Cardinal): string;
		var
			U: TUnitDebugInfos;
		begin
			if UseDebugInfos then
				begin
					U:= UnitWhichContainsAddress(Address - $1000 - Displ);

					if U <> Nil then
						Result:= 'Module ' + U.Name + RoutineWhichContainsAddress(Address - $1000 - Displ) + U.LineWhichContainsAddress(Address - $1000 - Displ)
					else
						Result:= RoutineWhichContainsAddress(Address - $1000 - Displ);
				end
			else
				Result:= '';
		end;

	procedure LogCallStack(var F: Text);
		var
			i: integer;
			_EBP : Integer;
			_ESP : Integer;
		begin
			asm
				mov  eax, ESP
				mov  _ESP,eax
				mov  eax, EBP
				mov  _EBP, eax
			end;

			_ESP:= _EBP;
			_EBP:= PInteger(_EBP)^;

			if (_EBP<_ESP) or (_EBP-_ESP>30000) then
				Exit;

			i:= 0;

			while i < 25 do
				begin
					_ESP:= _EBP;
					_EBP:= PInteger(_EBP)^;

					if (_EBP<_ESP) or (_EBP-_ESP>30000) then
						Exit;

					Writeln(F, #9 + IntToHex(PInteger(_EBP+4)^-4-$400000,8));

					i:= i + 1;
				end;
		end;

	type
		TExceptionProc = procedure(Exc: Exception; Addr: Integer);

	var
		InitialExceptionProc : TExceptionProc;
		VersionInfo: string;

	procedure MyExceptProc(Exc: Exception; Addr : Integer);
		begin
			Writeln(SevereExceptionsLogFile, '');
			Writeln(SevereExceptionsLogFile, '********* Severe exception detected - ' + DateTimeToStr(Now) + ' *********');
			Writeln(SevereExceptionsLogFile, VersionInfo);
			Writeln(SevereExceptionsLogFile, 'Exception code: ' + Exc.ClassName);
			Writeln(SevereExceptionsLogFile, 'Exception address: ' + IntToHex(Addr-$400000,8));
			Writeln(SevereExceptionsLogFile, #13#10'Call stack (oldest call at bottom):');
			LogCallStack(SevereExceptionsLogFile);
			Writeln(SevereExceptionsLogFile, '*****************************************************************');
			Writeln(SevereExceptionsLogFile, '');

			InitialExceptionProc(Exc, Addr);

			{The closing of the file is done in the finalization}
		end;

	procedure LogSevereExceptions(const WithVersionInfo: string);
		const
			FileNameBufSize = 1000;
		var
			LogFileName: string;
		begin
			if ExceptProc <> @MyExceptProc then
				{not installed yet ?}
				begin
					try
						SetLength(LogFileName, FileNameBufSize);
						GetModuleFileName(0, PChar(LogFileName), FileNameBufSize);
						LogFileName:= copy(LogFileName, 1, pos('.', LogFileName)) + 'log';

						AssignFile(SevereExceptionsLogFile, LogFileName);

						if FileExists(LogFileName) then
							Append(SevereExceptionsLogFile)
						else
							Rewrite(SevereExceptionsLogFile);
					except
						Exit;
					end;

					InitialExceptionProc:= ExceptProc;
					ExceptProc := @MyExceptProc;
					VersionInfo:= WithVersionInfo;
				end;
		end;

	function IsMemCheckActive: boolean;
		begin
			Result:= MemCheckActive
		end;

	constructor TUnitDebugInfos.Create(const AName: string; const NbLines: Cardinal);
		begin
			Name:= AName;

			{$IFDEF VER120}
			SetLength(Addresses, NbLines);
			{$ELSE}
			GetMem(Addresses, NbLines * 4);
			AddressesCount:= NbLines;
			{$ENDIF}
		end;

	{$IFNDEF VER120}
	destructor TUnitDebugInfos.Destroy;
		begin
			FreeMem(Addresses);

			inherited Destroy;
		end;
	{$ENDIF}

	constructor TRoutineDebugInfos.Create(const AName: string; const AStartAddress: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}; const ALength: Cardinal);
		begin
			Name:= AName;
			StartAddress:= AStartAddress;
			EndAddress:= StartAddress + ALength - 1;
		end;

	constructor TAddressToLine.Create(const AAddress, ALine: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF});
		begin
			Address:= AAddress;
			Line:= ALine
		end;

	function TUnitDebugInfos.LineWhichContainsAddress(const Address: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF}): string;
		var
			Start, Finish, Pivot: Cardinal;
		begin
			if Addresses[0].Address > Address then
				Result:= ''
			else
				begin
					Start:= 0;
					Finish:= {$IFNDEF VER120}AddressesCount{$ELSE}Length(Addresses) - 1{$ENDIF};

					while Start < Finish - 1 do
						begin
							Pivot:= Start + (Finish - Start) div 2;

							if Addresses[Pivot].Address = Address then
								begin
									Start:= Pivot;
									Finish:= Start
								end
							else
								if Addresses[Pivot].Address > Address then
									Finish:= Pivot
								else
									Start:= Pivot
						end;

					Result:= ' Line ' + IntToStr(Addresses[Start].Line);
				end;
		end;

	{$IFNDEF VER120}
	constructor TObjectList.Create;
		begin
			SetCapacity(100);
		end;
		
	destructor TObjectList.Destroy;
		begin
			Clear;

			inherited;
		end;

	procedure TObjectList.Add(const O: TObject);
		begin
			if Count = Capacity then
				Grow;
			fData^[Count]:= O;
			fCount:= Count + 1;
		end;

	procedure TObjectList.Clear;
		begin
			SetCount(0);
			SetCapacity(0);
		end;

	function TObjectList.Get(const Index: Integer): TObject;
		begin
			Assert((Index >= 0) and (Index < Count), 'TObjectList.Get: out of bounds');

			Result:= fData^[Index];
		end;

	procedure TObjectList.Grow;
		begin
			SetCapacity(Capacity * 2);
		end;

	procedure TObjectList.SetCapacity(const C: Integer);
		begin
			Assert((C >= Count) and (C < MaxListSize), 'TObjectList.SetCapacity: out of range');

			if C <> Capacity then
				begin
					ReallocMem(fData, C * SizeOf(Pointer));
					fCapacity:= C;
				end;
		end;

	procedure TObjectList.SetCount(const C: Integer);
		begin
			Assert((C >= 0) and (C < MaxListSize), 'TObjectList.SetCount: out of range');

			if C > Capacity then
				SetCapacity(C);

			if C > Count then
				FillChar(fData^[Count], (C - Count) * SizeOf(Pointer), 0);

			fCount:= C;
		end;
	{$ENDIF}
	
	type
		SRCMODHDR = packed record
			_cFile  : Word;
			_cSeg	: Word;
			_baseSrcFile : array [0..MaxListSize] of Integer;
		end;

		SRCFILE = packed record
			_cSeg	: Word;
			_nName	: Integer;
			_baseSrcLn : array [0..MaxListSize] of Integer;
		end;

		SRCLN = packed record
			_Seg	: Word;
			_cPair	: Word;
			_Offset : array [0..MaxListSize] of Integer;
		end;

		PSRCMODHDR = ^SRCMODHDR;
		PSRCFILE = ^SRCFILE;
		PSRCLN = ^SRCLN;

		TArrayOfByte = array [0..MaxListSize] of Byte;
		TArrayOfWord = array [0..MaxListSize] of Word;
		PArrayOfByte = ^TArrayOfByte;
		PArrayOfWord = ^TArrayOfWord;
		PArrayOfPointer = ^TArrayOfPointer;
		TArrayOfPointer = array [0..MaxListSize] of PArrayOfByte;

	procedure AddRoutine(const Name: string; const Start, Len: Cardinal);
		begin
			{$IFNDEF VER120}
			Routines.Add(TRoutineDebugInfos.Create(Name, Start, Len));
			{$ELSE}
			if Length(Routines) <= RoutinesCount then
				SetLength(Routines, Max(RoutinesCount * 2, 1000));

			Routines[RoutinesCount]:= TRoutineDebugInfos.Create(Name, Start, Len);
			RoutinesCount:= RoutinesCount + 1;
			{$ENDIF}
		end;

	{$IFDEF VER120}
	procedure AddUnit(const U: TUnitDebugInfos);
		begin
			if Length(Units) <= UnitsCount then
				SetLength(Units, Max(UnitsCount * 2, 1000));

			Units[UnitsCount]:= U;
			UnitsCount:= UnitsCount + 1;
		end;
	{$ENDIF}

	procedure dumpsymbols(NameTbl : PArrayOfPointer; sstptr : PArrayOfByte; size : integer);
		//Copyright (C) Tenth Planet Software Intl., Clive Turvey 1998. All rights reserved. - Reused & modified by SG with permission
		var
			len, sym : integer;
		begin
			while size > 0 do
				begin
					len := PWord(@sstptr^[0])^;
					sym := PWord(@sstptr^[2])^;

					INC(len,2);

					if ((sym=$205) or (sym=$204)) and (PInteger(@sstptr^[40])^ > 0) then
						AddRoutine(PChar(NameTbl^[PInteger(@sstptr^[40])^ - 1]), PInteger(@sstptr^[28])^, PInteger(@sstptr^[16])^);

					if (len=2) then
						size := 0
					else
						begin
							sstptr := @sstptr^[len];
							DEC(size,len);
						end;
				end;
		end;

	procedure dumplines(NameTbl : PArrayOfPointer; sstptr : PArrayOfByte; size : word);
		//Copyright (C) Tenth Planet Software Intl., Clive Turvey 1998. All rights reserved. - Reused & modified by SG with permission
		var
			srcmodhdr	: PSRCMODHDR;
			i			: Word;
			srcfile		: PSRCFILE;
			srcln 		: PSRCLN;
			k			: Word;
			CurrentUnit: TUnitDebugInfos;
		begin
			if size > 0 then
				begin
					srcmodhdr := PSRCMODHDR(sstptr);

					for i:=0 to pred(srcmodhdr^._cFile) do
						begin
							srcfile := PSRCFILE(@sstptr^[srcmodhdr^._baseSrcFile[i]]);

							if srcfile^._nName > 0 then
								//note: I assume that the code is always in segment #1. If this is not the case, Houston !  - VM
								begin
									srcln := PSRCLN(@sstptr^[srcfile^._baseSrcLn[0]]);

									CurrentUnit:= TUnitDebugInfos.Create(ExtractFileName(PChar(NameTbl^[srcfile^._nName-1])), srcln^._cPair);
									{$IFNDEF VER120}
									Units.Add(CurrentUnit);
									{$ELSE}
									AddUnit(CurrentUnit);
									{$ENDIF}

									for k:=0 to pred(srcln^._cPair) do
										CurrentUnit.Addresses[k]:= TAddressToLine.Create(Integer(PArrayOfPointer(@srcln^._Offset[0])^[k]), Integer(PArrayOfWord(@srcln^._Offset[srcln^._cPair])^[k]));
								end;
						end;
				end;
		end;

	procedure GetProjectInfos;
		//Copyright (C) Tenth Planet Software Intl., Clive Turvey 1998. All rights reserved. - Reused & modified by SG with permission
		var
			AHeader : Packed record
				Signature : Array[0..3] of Char;
				AnInteger : Integer;
			end;
			k	: integer;
			j 	: Word;
			lfodir : Integer;
			SstFrameSize: integer;
			SstFrameElem: PArrayOfByte;
			ssttype, sstsize, sstbase : Integer;
			x, y, z : Integer;
			sstbuf 	: PArrayOfByte;
			OldFileMode: integer;
			AFileOfByte : file of Byte;
			Names 	: PArrayOfByte;
			NameTbl : PArrayOfPointer;
			SstFrame: PArrayOfByte;
			ifabase			: Integer;
			cdir, cbdirentry: word;
		begin
			{$IFNDEF VER120}
			Routines:= TObjectList.Create;
			Units:= TObjectList.Create;
			{$ELSE}
			RoutinesCount:= 0;
			UnitsCount:= 0;
			{$ENDIF}

			OldFileMode:= FileMode;
			FileMode:= 0;

			AssignFile(AFileOfByte, ParamStr(0));
			Reset(AFileOfByte);

			Names:= Nil;
			NameTbl:= Nil;
			Seek(AFileOfByte,FileSize(AFileOfByte)-SizeOf(AHeader));
			BlockRead(AFileOfByte,AHeader, SizeOf(AHeader));
			if (AHeader.Signature <> 'FB09') and (AHeader.Signature <> 'FB0A') then
				exit;
			ifabase := FilePos(AFileOfByte) - AHeader.AnInteger;
			Seek(AFileOfByte,ifabase);
			BlockRead(AFileOfByte,AHeader, SizeOf(AHeader));
			if (AHeader.Signature <> 'FB09') and (AHeader.Signature <> 'FB0A') then
				exit;
			lfodir := ifabase + AHeader.AnInteger;
			if lfodir < ifabase then
				exit;

			Seek(AFileOfByte,lfodir);
			BlockRead(AFileOfByte, j, SizeOf(Word));
			BlockRead(AFileOfByte, cbdirentry, SizeOf(Word));
			BlockRead(AFileOfByte, cdir, SizeOf(Word));
			Seek(AFileOfByte,lfodir+j);

			SstFrameSize := cdir * cbdirentry;
			getmem(SstFrame,SstFrameSize);
			BlockRead(AFileOfByte,SstFrame^,SstFrameSize);

			for k:=0 to pred(cdir) do
			begin
				SstFrameElem := @SstFrame^[k * cbdirentry];
				ssttype := PWord(@SstFrameElem^[0])^;
				if (ssttype=$0130) then
				begin
					sstbase := ifabase + PInteger(@SstFrameElem^[4])^;
					sstsize := PInteger(@SstFrameElem^[8])^;
					getmem(Names,sstsize);
					Seek(AFileOfByte,sstbase);
					BlockRead(AFileOfByte,Names^,sstsize);
					y := PInteger(@Names^[0])^;
					getmem(NameTbl,sizeof(Pointer) * y);
					z := 4;
					for x:=0 to pred(y) do
					begin
						NameTbl^[x] := @Names^[z + 1];
						z := z + Names^[z] + 2;
					end;
				end;
			end;

			for k:=0 to pred(cdir) do
			begin
				SstFrameElem := @SstFrame^[k * cbdirentry];
				ssttype := PWord(@SstFrameElem^[0])^;

				sstbase := ifabase + PInteger(@SstFrameElem^[4])^;
				sstsize := PInteger(@SstFrameElem^[8])^;
				getmem(sstbuf,sstsize);
				Seek(AFileOfByte,sstbase);
				BlockRead(AFileOfByte,sstbuf^,sstsize);

				if (ssttype=$0125) then
					dumpsymbols(NameTbl, @sstbuf^[4],sstsize - 4);

				if (ssttype=$0127) then
					dumplines(NameTbl, sstbuf,sstsize);

				FreeMem(sstbuf);
			end;

			FreeMem(Names);
			FreeMem(NameTbl);
			FreeMem(SstFrame);

			CloseFile(AFileOfByte);
			FileMode:= OldFileMode;
		end;

	procedure InitializeOnce;
		var
			i: integer;
		begin
			if not MemCheckInitialized then
				{once mechanism}
				begin
                	{We verify that the necessary project options are checked in order for memcheck to work properly}

					{$IFOPT O+}
					MessageBox(0, 'The project was compiled with optimization - MemCheck can not be used', 'MemCheck: abnormal termination', MB_OK or MB_ICONERROR or MB_TASKMODAL);
					ExitProcess($FFFF);
					{$ENDIF}
					{$IFOPT W-}
					MessageBox(0, 'The project was compiled without stack frames - MemCheck can not be used', 'MemCheck: abnormal termination', MB_OK or MB_ICONERROR or MB_TASKMODAL);
					ExitProcess($FFFF);
					{$ENDIF}
					{$IFOPT D-}
					MessageBox(0, 'The project was compiled without debug information - MemCheck can not be used', 'MemCheck: abnormal termination', MB_OK or MB_ICONERROR or MB_TASKMODAL);
					ExitProcess($FFFF);
					{$ENDIF}
					{$IFOPT L-}
					MessageBox(0, 'The project was compiled without local symbols - MemCheck can not be used', 'MemCheck: abnormal termination', MB_OK or MB_ICONERROR or MB_TASKMODAL);
					ExitProcess($FFFF);
					{$ENDIF}
					{$IFOPT Y-}
					MessageBox(0, 'The project was compiled without symbol information - MemCheck can not be used', 'MemCheck: abnormal termination', MB_OK or MB_ICONERROR or MB_TASKMODAL);
					ExitProcess($FFFF);
					{$ENDIF}

					OutOfMemory:= EOutOfMemory.Create('Memcheck is not able to allocate memory, due to system resource lack');
					HeapCorrupted:= Exception.Create('Heap corrupted');
					ChangeFinalizationsOrder;
					MemCheckInitialized:= True;
					GIndex:= 0;
					LastBlock:= Nil;
					FreedInstance:= Pchar(ReleasedInstance)-52;
					Move(FreedInstance^, BadObjectVMT.A, 48);
					FreedInstance:= PChar(@BadObjectVMT.B[1]);
					for I:= 0 TO MaxNbSupportedVMTEntries do
						begin
							BadObjectVMT.B[I]:= PChar(@ReleasedInstance.Error)+6*I;
							BadInterfaceVMT[I]:= PChar(@ReleasedInstance.InterfaceError);
						end;
					if IdentifyObjectFields then
						CurrentlyAllocatedBlocksTree:= TIntegerBinaryTree.Create;

					GetProjectInfos;
				end;
		end;

	procedure OutputAllocatedBlocks;
		var
			Block: PMemoryBlocHeader;
			BlockRepresentation: string;
			i: Integer;
			TotalLeak: {$IFDEF VER120}cardinal{$ELSE}Integer{$ENDIF};
			ShowOutput: Boolean;
			OutputFile: Text;

			ChronologicalLeaksList: PStringsArray;
			ChronologicalLeaksCount: integer;
			ChronologicalLeaksAllocatedSpace: integer;
				{an ordered string list containing the chronological info about the leaks
				this could be a tstringlist, but we do not want to have a "uses classes"}

			AllocatedBlocksText: PStringsArray;
			AllocatedBlocksCounter: PIntegersArray;
			AllocatedBlocksCount: integer;
			AllocatedBlocksReservedMem: integer;
				{This contains the list of blocks currently allocated.
				The strings are exactly the output
				The objects are a counter of instances of this leak.
				The leak id is the index of the leak in this list}
		procedure Output(const S: string);
			{outputs S and a CRLF in the output file}
			begin
				WriteLn(OutputFile, S);
			end;
		function IndexOfAllocatedBlockText(const BlockText: string): integer;
			begin
				Result:= 0;
				while (Result < AllocatedBlocksCount) and (AllocatedBlocksText[Result] <> BlockText) do
					Result:= Result + 1;
			end;
		procedure AddAllocatedBlock(const BlockText: string);
			var
				i: integer;
			begin
				i:= 0;
				while (i < AllocatedBlocksCount) and (AllocatedBlocksText[i] <> BlockText) do
					i:= i + 1;

				if i = AllocatedBlocksCount then
					begin
						if AllocatedBlocksCount = AllocatedBlocksReservedMem then
							begin
								if AllocatedBlocksReservedMem = 0 then
									AllocatedBlocksReservedMem:= 10;
								AllocatedBlocksReservedMem:= AllocatedBlocksReservedMem * 2;
								UnMemChk;
								ReallocMem(AllocatedBlocksText, AllocatedBlocksReservedMem * sizeof(WideString));
								ZeroMemory(pointer(integer(AllocatedBlocksText) + AllocatedBlocksCount * sizeof(WideString)), (AllocatedBlocksReservedMem - AllocatedBlocksCount) * SizeOf(WideString));
								ReallocMem(AllocatedBlocksCounter, AllocatedBlocksReservedMem * sizeof(Integer));
								MemChk;
							end;

						AllocatedBlocksText[AllocatedBlocksCount]:= BlockText;
						AllocatedBlocksCounter[AllocatedBlocksCount]:= 1;
						AllocatedBlocksCount:= AllocatedBlocksCount + 1;
					end
				else
					AllocatedBlocksCounter[i]:= AllocatedBlocksCounter[i] + 1;
			end;
		procedure AddChronologicalLeak(const LeakText: string);
			begin
				if ChronologicalLeaksCount = ChronologicalLeaksAllocatedSpace then
					begin
						if ChronologicalLeaksAllocatedSpace = 0 then
							ChronologicalLeaksAllocatedSpace:= 10;
						ChronologicalLeaksAllocatedSpace:= ChronologicalLeaksAllocatedSpace * 2;
						UnMemChk;
						ReallocMem(ChronologicalLeaksList, ChronologicalLeaksAllocatedSpace * sizeof(WideString));
						ZeroMemory(pointer(integer(ChronologicalLeaksList) + ChronologicalLeaksCount * sizeof(WideString)), (ChronologicalLeaksAllocatedSpace - ChronologicalLeaksCount) * SizeOf(WideString));
						MemChk;
					end;

				ChronologicalLeaksList[ChronologicalLeaksCount]:= LeakText;
				ChronologicalLeaksCount:= ChronologicalLeaksCount + 1;
			end;
		begin
        	InitializeOnce;

			AssignFile(OutputFile, MemCheckLogFileName + '.$$$');
			Rewrite(OutputFile);

			ChronologicalLeaksCount:= 0;
			ChronologicalLeaksAllocatedSpace:= 0;
			ChronologicalLeaksList:= Nil;

			AllocatedBlocksText:= Nil;
			AllocatedBlocksCounter:= Nil;
			AllocatedBlocksCount:= 0;
			AllocatedBlocksReservedMem:= 0;

			TotalLeak:= 0;
			ShowOutput:= False;

			{step 1: we collect the list of allocated blocks in a hash table}
			Block:= LastBlock;
			while (Block <> Nil) and (IgnoreMaxLeak or (AllocatedBlocksCount < MaxLeak)) do
				begin
					if BlocksToShow[Block.KindOfBlock] then
						begin
							if Block.DestructionAdress = Nil then
								{this is a leak}
								begin
									BlockRepresentation:= BlockDescription[Block.KindOfBlock];

									if Block.KindOfBlock = MClass then
										BlockRepresentation:= BlockRepresentation + TObject(PChar(Block) + SizeOf(TMemoryBlocHeader)).ClassName + ', Size: ' + IntToStr(Block.AllocatedSize) + ', allocated at ' + IntToHex(Cardinal(Block.CallerAddress[0])-Displ,8) + ' ' + TextualDebugInfoForAddress(integer(Block.CallerAddress[0])) + #13#10
									else
										BlockRepresentation:= BlockRepresentation + ', Size: ' + IntToStr(Block.AllocatedSize) + ', allocated at ' + IntToHex(Cardinal(Block.CallerAddress[0])-Displ,8) + ' ' + TextualDebugInfoForAddress(integer(Block.CallerAddress[0])) + #13#10 + MemoryBlockDump(Block) + #13#10;

									if IdentifyObjectFields then
										for i:= 0 to NotDestroyedFieldsCount - 1 do
											if pointer(NotDestroyedFields[i]) = Block then
												BlockRepresentation:= BlockRepresentation + '     Was field # ' + IntToStr(TFieldInfo(NotDestroyedFieldsInfos[i]).FieldIndex) + ' of a ' + TFieldInfo(NotDestroyedFieldsInfos[i]).OwnerClass.ClassName + #13#10;

									for i:= 1 to StoredCallStackDepth do
										if Integer(Block.CallerAddress[i])>0 then
											BlockRepresentation:= BlockRepresentation + '     call stack -' + chr(ord('0')+i) + ' : ' + IntToHex(Cardinal(Block.CallerAddress[i])-Displ,8) + ' ' + TextualDebugInfoForAddress(integer(Block.CallerAddress[i])) + #13#10;

									BlockRepresentation:= BlockRepresentation + #13#10;

									AddAllocatedBlock(BlockRepresentation);

									AddChronologicalLeak(BlockRepresentation);
									TotalLeak:= TotalLeak + Block.AllocatedSize;
								end
							else
								{this is not a leak}
								if CheckWipedBlocksOnTermination and (Block.AllocatedSize > 5) and (Block.AllocatedSize <= DoNotCheckWipedBlocksBiggerThan) and (not IsMemFilledWithChar(pchar(Block) + SizeOf(TMemoryBlocHeader) + 4, Block.AllocatedSize - 5, CharToUseToWipeOut)) then
									begin
										Output('SEVERE ERROR: Destroyed block damaged - Block allocated at ' + IntToHex(Cardinal(Block.CallerAddress[0])-Displ,8) + ' - destroyed at ' + IntToHex(Cardinal(Block.DestructionAdress),8));
										ShowOutput:= True;
									end;
						end;

					Block:= Block.PreceedingBlock;
				end;

			{step 2: we output the collected information}
			Output(#13#10 + 'Total leak: ' + IntToStr(TotalLeak) + #13#10#13#10);
			Output(#13#10#13#10 + '*** MEMCHK: Blocks STILL allocated ***' + #13#10);
			for i:= 0 to AllocatedBlocksCount - 1 do
				Output('Leak #' + IntToStr(i) + ' - ' + IntToStr(AllocatedBlocksCounter[i]) + ' occurence(s) - ' + AllocatedBlocksText[i]);
			Output('*** MEMCHK: End of allocated blocks ***' + #13#10#13#10#13#10);

			{step 3: we give chronological info}
			Output('*** MEMCHK: chronological leak information ***' + #13#10);
			for i:= 0 to ChronologicalLeaksCount - 1 do
				Output(IntToStr(i + 1) + ' - ' + Copy(ChronologicalLeaksList[i], 1, Pos(#13#10, ChronologicalLeaksList[i]) - 1) + ' - Leak #' + IntToStr(IndexOfAllocatedBlockText(ChronologicalLeaksList[i])));
			Output(#13#10 + '*** MEMCHK: end of chronological leak information ***' + #13#10#13#10#13#10);

			FreeMem(ChronologicalLeaksList);

			{step 4: Output the time stamp infos}
			Output(#13#10#13#10 + '*** MEMCHK: Time stamps ***' + #13#10);
			for i:= 0 to TimeStampsCount - 1 do
				Output(TimeStamps[i]);
			Output(#13#10 + '*** MEMCHK: end of time stamps ***' + #13#10#13#10#13#10);

			{step 5: we output the allocation stats if necessary}
			if CollectStatsAboutObjectAllocation then
				begin
					Output(#13#10#13#10 + '*** MEMCHK: Allocation stats ***' + #13#10);
					Output('Nb instances'#9'Instance size'#9'ClassName'#13#10);
					for i:= 0 to AllocStatsCount - 1 do
						Output(IntToStr(AllocatedInstances[i]) + #9#9 + IntToStr(AllocatedObjectsClasses[i].InstanceSize) + #9#9 + AllocatedObjectsClasses[i].ClassName);
					Output(#13#10 + '*** MEMCHK: end of allocation stats ***' + #13#10#13#10#13#10);
				end;

			if ComputeMemoryUsageStats then
				begin
					Output(#13#10#13#10 + '*** MEMCHK: Memory usage stats ***' + #13#10);
					for i:= 0 to MemoryUsageStatsCount - 1 do
						Output(IntToStr(MemoryUsageStats[i]));
					Output(#13#10 + '*** MEMCHK: end of memory usage stats ***' + #13#10#13#10#13#10);
				end;

			if KeepMaxMemoryUsage then
					Output(#13#10 + '*** Biggest memory usage was: ' + IntToStr(MaxMemoryUsage) + ' ***' + #13#10#13#10#13#10);

			{step 6: we save and show the output}
			Close(OutputFile);
			if FileExists(MemCheckLogFileName) then
				DeleteFile(MemCheckLogFileName);
			Rename(OutputFile, MemCheckLogFileName);
			if ShowOutput or (AllocatedBlocksCount > 0) or CollectStatsAboutObjectAllocation or ComputeMemoryUsageStats or KeepMaxMemoryUsage then
				WinExec(PChar('notepad ' + MemCheckLogFileName), sw_Show);
			FreeMem(AllocatedBlocksText);
			FreeMem(AllocatedBlocksCounter);
		end;

	procedure AddTimeStampInformation(const I: string);
		begin
			InitializeOnce;
			
			if TimeStampsCount = TimeStampsAllocated then
				begin
					if TimeStampsAllocated = 0 then
						TimeStampsAllocated:= 10;
					TimeStampsAllocated:= TimeStampsAllocated * 2;

					UnMemChk;
					ReallocMem(TimeStamps, TimeStampsAllocated * sizeof(WideString));
					ZeroMemory(pointer(integer(TimeStamps) + TimeStampsCount * sizeof(WideString)), (TimeStampsAllocated - TimeStampsCount) * SizeOf(WideString));
					MemChk;
				end;

			TimeStamps[TimeStampsCount]:= I + ' - Time stamp: ' + IntToStr(TimeStampsCount);
			TimeStampsCount:= TimeStampsCount + 1;
		end;

	procedure MemChk;
		const
			LeakTrackingMemoryManager: TMemoryManager = (
				GetMem: LeakTrackingGetMem;
				FreeMem: LeakTrackingFreeMem;
				ReallocMem: LeakTrackingReallocMem;
			   );
			HeapCheckingMemoryManager: TMemoryManager = (
				GetMem: HeapCheckingGetMem;
				FreeMem: HeapCheckingFreeMem;
				ReallocMem: HeapCheckingReallocMem;
			   );
		begin
			assert(sizeof(TMemoryBlocHeader) mod 8 = 0, 'SizeOf(TMemoryBlocHeader) in MemCheck should be a multiple of 8');
			InitializeOnce;
			if CheckHeapStatus then
				begin
					SetMemoryManager(HeapCheckingMemoryManager);
					UpdateLastHeapStatus;
				end
			else
				SetMemoryManager(LeakTrackingMemoryManager);
			MemCheckActive:= True;
		end;

	procedure CommitReleases;
		var
			Block, BlockToFree, previous: PMemoryBlocHeader;
		begin
			InitializeOnce;

			Block:= LastBlock;
			Previous:= nil;

			while Block <> Nil do
				begin
					BlockToFree:= Block;
					Block:= Block.PreceedingBlock;

					if BlockToFree.DestructionAdress <> Nil then
						begin
							if LastBlock = BlockToFree then
								LastBlock:= Block;

							if previous <> nil then
								previous.PreceedingBlock:= Block;

							SysFreeMem(BlockToFree);
						end
					else
						previous:= BlockToFree;
				end;
		end;

	procedure dummy;
		{This procedure is never called. It is used for computing the address of MemCheck's finalization.
		Hence, it MUST be just before the finalization and be empty. If you want to change that, you'll have
		to change the way memcheck's finalization is seeked}
		begin
		end;

initialization
finalization
	if ExceptProc = @MyExceptProc then
		{Exception logger installed}
		Close(SevereExceptionsLogFile);

	if MemCheckInitialized then
		begin
			if MemCheckActive then
				begin
					UnMemChk;
					OutputAllocatedBlocks;
					GoThroughAllocatedBlocks;
				end;

			FreeMem(TimeStamps);
			FreeMem(AllocatedObjectsClasses);
			FreeMem(AllocatedInstances);
			OutOfMemory.Destroy;
			if FileExists(MemCheckLogFileName + '.$$$') then
				DeleteFile(MemCheckLogFileName + '.$$$');
		end;
end.

