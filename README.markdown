# BNRPersistence
Aaron Hillegass<br/>
aaron@bignerdranch.com<br/>
July 9, 2010

After a few years of whining that Core Data could have been better, I thought I should write a persistence framework that points in what I think is the right direction.  And this is it.  BNRPersistence is similar to both archiving and Core Data.

One big difference? Objects can have ordered relationships.  For example, a playlist of songs is an ordered collection.  This is awkward to do in Core Data because it uses a relational database.

Another big difference? It doesn't use SQLite, but rather a key-value store called Tokyo Cabinet.

BNRPersistence is not really a framework at the moment,  just a set of classes that you can include in your project.

Features include: 

- better performance than Core Data in many situations
- indexed full-text search
- encrypted data stores
- per-instance versioning.

## Installation

First, you need to download Tokyo Cabinet: [http://1978th.net/tokyocabinet/]()
(There is a sourceforge page, but the latest build seems to be on this site.)

In Terminal.app, untar the tarball and cd into the resulting directory.

(You want 64-bit library? In Terminal, set an environment variable for 64-bit builds:

    export CFLAGS='-arch x86_64'

)

Configure, build, and install:

	./configure
	make
	sudo make install

Now, you have a /usr/local/lib/libtokyocabinet.a that needs to linked into any project that uses these classes. (I usually use the "Add->Existing Frameworks" menu item to do this.)

You'll also need to have /usr/local/include/ among your header search path. (See the Xcode target's build info to add this.)

If you want full-text searching, you will also need to install Tokyo Dystopia.  If you do not want full-text searching, leave out BNRTCIndexManager.

Now just add the classes in the BNRPersistence directory into your project.  (If you copy them, you won't get the new version when you update your git repository.  This may be exactly what you want, but these are pretty immature, so I would suggest that you link to them instead.)

## Using It

You've used Core Data?  The BNRStore is analogous to NSManagedObjectContext.  BNRStoredObject is analogous to NSManagedObject.  There is no model, instead, like archiving, you must have two methods in you BNRStoredObject subclass.  Here are the methods from a Playlist class:

	- (void)readContentFromBuffer:(BNRDataBuffer *)d
	{
	    [title release];
	    title = [[d readString] retain];
    
	    [songs release];
	    songs = [d readArrayOfClass:[Song class]
	                     usingStore:[self store]];
	    [songs retain];
	}

	- (void)writeContentToBuffer:(BNRDataBuffer *)d
	{
	    [d writeString:title];
	    [d writeArray:songs ofClass:[Song class]];
	}

So, BNRDataBuffers are like NSData, but they have methods for reading and writing different types of data. (It does byte-swapping so you can move the data files from PPC to Intel without a problem.)

(I certainly debate the idea of a model file that would replace these methods, but for now this is fast and simple.  If you would rather have a model file than implement these methods, you are invited to write a BNRPersistenceModelEditor.)

All the instances of class will be stored in a single TokyoCabinet file, thus you will be saving to a directory containing one file for each class that you are storing.

To create a store, first you must create a backend.  I have tried BerkeleyDB and GDBM, but I am quite enamored with Tokyo Cabinet right now.  (If you would like to try the other backends, write me and I'll send you the necessary classes.)

    NSError *error;
    NSString *path = @"/tmp/complextest/";
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
                                                         error:&error];
    if (!backend) {
        NSLog(@"Unable to create database at %@", path);
        …display error here… 
    }

Now that you have a backend, you can create a store and tell it which classes you are going to be saving or loading:

    BNRStore *store = [[BNRStore alloc] init];
    [store setBackend:backend];
    [backend release];
    
    [store addClass:[Song class]];
    [store addClass:[Playlist class]];

(You must add the classes in the same order every time.  The classID of a class (see BNRClassMetaData) is determined by the order.)

Now to get a list of of the playlists in the store:

    NSArray *allPlaylists = [store allObjectsForClass:[Playlist class]];

To insert a new playlist:

      Playlist *playlist = [[Playlist alloc] init];
      [store insertObject:playlist];

To delete a playlist:

      [store deleteObject:playlist];

To update a playlist:

      [store willUpdateObject:playlist];
      [playlist setTitle:@"CarMusic"];

(Yes, this is a place where a model file would make the framework cooler: I could become an observer of this object and get notified when the value changed.)

(As a side-note, there is some experimental support for automatically updating the undo managed.  Just give the store an undo manager.  This also could be made better with a model file.)

To save all the changes:

    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
        return -1;
    }

Each class in a store can have a version.  This is kept in the BNRClassMetaData object for the class.  You can reach this in your readContentFromBuffer method (because you have access to the store).

In your BNRStoredObject class, you can implement these two methods if you wish:

	- (void)prepareForDelete;
	- (void)dissolveAllRelationships;

prepareForDelete is where you implement your delete rule: When a song is deleted, for example, it needs to remove itself from any playlists it is in.

dissolveAllRelationships is a fix for a common problem.  You close the document, but the objects in the document have retain cycles so they don't get deallocated properly.  In dissolveAllRelationships,  your stored objects (the ones in memory) are being asked to release any other stored objects they are retaining.

In the directory, you will find TCSpeedTest and CDSpeedTest. These are command-line tools that compare the speed of some tasks in BNRPersistence (TCSpeedTest) and Core Data (CDSpeedTest)

Your mileage may vary,  but I see:

Creating 1,000 playlists, 100,000 songs, and 100 songs in each playlist and saving (ComplexInsertTest):
BNRPersistence is 10 times faster than CoreData

Reading in the playlist and getting the title of the first song in each playlist (ComplexFetchTest):
BNRPersistence is 13 times faster than CoreData

Creating 1,000,000 songs and inserting them and saving (SimpleInsertTest):
BNRPersistence is 17 times faster than CoreData

Fetching in 1,000,000 songs (SimpleFetchTest)
CoreData is 2 times faster than BNRPersistence
(BNRPersistence is single-threaded and CoreData has some clever multi-threading in this case.  I think I can do similar tricks in BNRPersistence and catch up in this case.  In either case, it is very, very fast.  On my machine, fetching a million songs takes 6.2 seconds the first time and 3.9 seconds the second time.)

## Full-Text Search

To do full-text search, you need to give the BNRStore an instance of BNRTCIndexManager.  I usually have it put the index in the same directory as the data itself:

    BNRStore *store = ...    
    NSError *err;
    BNRTCIndexManager *indexManager = [[BNRTCIndexManager alloc] initWithPath:@"/tmp/foo"
                                                                        error:&err];
    [store setIndexManager:indexManager];
    [indexManager release];

Then the BNRStoredObject subclass identifies which attributes need to be full-text indexed:

	+ (NSSet *)textIndexedAttributes
	{
	    static NSSet *textKeys = nil;
	    if (!textKeys) {
	        textKeys = [[NSSet alloc] initWithObjects:@"title", nil];
	    }
	    return textKeys;
	}

Now you can search using Tokyo-Dystopia-style search strings:

    NSMutableArray *songsThatMatch = [store objectsForClass:[Song class]
                                              matchingText:@"[[*choice*]]"
                                                    forKey:@"title"];

On my system, I can search 1M records and fetch the 727 that match in 0.1 seconds.  The same operation on Core Data takes 1.3 seconds.

## Versioning

Over time, the data that you choose to write for an object may change.  That is, in version 1.0, a person has only a phone number.  In version 2.0, you add a fax number.  To support this, there is versioning.   Before you write any instances out, set the version number on the class:

	[Song setVersion:2];

When reading in a data buffer, check the versionOfData before trying to read the fax number;

	- (void)readContentFromBuffer:(BNRDataBuffer *)d
	{
	    [phone release];
	    phone = [[d readString] retain];
    
	    if ([d versionOfData] > 2) {
	        fax = [[d readString] retain];
	    }
	}

The version is an 8-bit unsigned integer which is stored with the object's data.  (Yes, you get a version number for each instance. Thus, you don't need to upgrade all the objects simultaneously. As they are edited and saved out, they will be automatically upgraded.)

I recognize that at times graceful incremental upgrades may not be possible (rather the entire directed graph of objects must be upgraded), but I wanted to make possible the many cases incremental upgrades are desireable.

(You can disable the per-instance versioning if you must:

	[store setUsesPerInstanceVersioning:NO];

This is provided primarily for backwards compatibility with apps that started using this framework before I added per-instance-versioning.)

## Encryption

BNRPersistence supports encryption of individual objects within the database.  Simply set the encryption key to use:

    BNRStore *store = [[BNRStore alloc] init];
    [store setEncryptionKey:@"the passphrase"];
    ...

Technical details of the encryption system used:

- The [AES128][] symmetric block cipher is used.  The implementation is provided by Apple's CommonCrypto, which is available for both Mac OS X and iPhone OS.
- Objects are encrypted one-by-one; the database as a whole is _not_ encrypted.  This means that a person inspecting the database could see that there are 18 records of class Person, but not the contents of those records.
- BNRPersistence salts the actual key used with a random value combined with the rowID (primary key) of each stored object.  As such, records with identical values will appear to be different to a person inspecting the database.  The record length will be the same, however.
- During decryption the salt for each object (along with the rowID) is used to verify that the decryption was successful.  If the decryption was deemed unsuccessful the stored object will be reconstituted using the original data buffer from the backend.  This allows you to add encryption to an existing database: new objects stored will be encrypted while old, unencrypted objects will still be accessible.

Also, note that the full-text indices are not encrypted.  So, you will typically use either encryption or full-text searching, but not both.

## The Size of Things

The database files are not small.  Nor are the full-text indexes.  For the million-song database, the Tokyo Cabinet data file is more than twice as large as the Core Data file.  And that doesn't include the index which is that also about twice as large as the Core Data file.

## Getting it on the Phone

The first problem is that you need to compile TokyoCabinet/TokyoDystopia for arm.  I tried every configure trick I could come up with and then just created an Xcode static library project and dumped the source to both libraries into the project.  This project is in the repository.

When you link to the resulting static library, you will also need to link in libz, which is part of the iPhone SDK

## Named data buffers

Saving the entire object graph in the file is great, but often you don't want to fetch out the whole graph, but rather start at some "bookmarked" point in that object graph.  So, I decided it would be handy to be able to give names to objects.  

But, then I thought, maybe instead of named objects, I should have named arrays.  Or named sets. Or named dictionaries.

Finally, I came up with a solution that looks a bit strange but can handle all these cases: Named Data Buffers.

You can load a data buffer with anything you like, and then store it under a name.  Here I'm saving a reference to BNRStoredObject under the name Favorite:

    Song *song = [[Song alloc] init];    [song setTitle:@"Walking on Sunshine"];    [song setSeconds:298];    [store insertObject:song];    BNRStoreBackend *backend = [store backend];    BNRDataBuffer *buf = [[BNRDataBuffer alloc] init];    [buf writeObjectReference:song];    [backend insertDataBuffer:buf forName:@"Favorite"];

Then, to fetch the object:
    BNRDataBuffer *buf = [backend dataBufferForName:@"Favorite"];    Song *song = [buf readObjectReferenceOfClass:[Song class]                                      usingStore:store];

Anything that can be put in a data buffer (and that is everything I can think of), can be given a name.

Note that you are talking directly to the backend, so the name change in the file is immediate.  In the example, the named reference to the song is now in the database, but the song itself won't be inserted into the database until I call saveChanges:.

## License

My code is under the MIT license and Tokyo Cabinet is under the LGPL.   I think this will enable you to use it how you want to use it.  If something bad happens because of the code, you can't sue us.  And if you make changes to Tokyo Cabinet, I think you need to submit those changes to the author. 

There has been some concern over the fact that Tokyo Cabinet and Tokyo Dystopia are under the LGPL.  I wrote to Mikio Hirabayashi, the author of Tokyo Cabinet and Tokyo Dystopia.  He made it very clear that he was fine with both libraries being used in commercial products. (However, if you read the LGPL, static linking seems to make your app a "derived work".  And, there is no dynamic linking on the iPhone.)

## To Do:

I recognize that there is room for improvement here:

0. The BNRDataBuffer should know how to write many more types of data
1. The creation of a model-file architecture
3. Use B+ trees to make attributes indexable and add support for NSPredicate
4. Better automatic undo support 
5. Automatic syncing to the cloud
6. Easy hooks for QuickLook images and Spotlight metadata in BNRStoreDocument
7. Hook it up to Tokyo Tyrant for non-local storage

[AES128]: http://en.wikipedia.org/wiki/Advanced_Encryption_Standard
