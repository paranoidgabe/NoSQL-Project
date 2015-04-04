// std::copy
#include <iostream>
#include <sys/stat.h>
#include <algorithm>
#include <cstring>
#include <cassert>

#include <iomanip>

#include <cstdio>

// std::min
#include <cmath>

#include "FileSystem.h"
#include "FileReader.h"
#include "FileWriter.h"
#include "File.h"

uint64_t depth = 0;
bool allEnabled = false;

template<uint64_t to>
uint64_t roundTo( uint64_t input ) {
    return to * ((input + to - 1)/to);
}

void tabs() {
    for( int i = 0 ; i < depth ; ++i ) std::cout << "\t";
}

template <class V>
void log( V msg , bool req = false ) {
    if( req || allEnabled ) {
        tabs();
        std::cout << msg << std::endl;                      
    }
}

template <class K,class V>
void log( K key , V value , bool req = false ) {
    if( req || allEnabled ) {
        tabs();
        std::cout << key << ": " << value << std::endl;
    }
}

#define enter(MSG) {    \
    log(MSG,true);      \
    ++depth;            \
}

#define leave(MSG) {    \
    --depth;            \
    log(MSG,true);      \
}

#define assertStream(stream) {              \
    assert( stream );                       \
    assert( stream.is_open() );             \
    assert( stream.fail() == false );       \
    assert( stream.bad() == false );        \
}

void gotoBlock( uint64_t block , uint64_t blockSize , std::fstream &stream ) {
    assertStream( stream );

    stream.seekp( block * blockSize );
    stream.seekg( block * blockSize );

    assertStream( stream );
}

void printFile( os::File &file , bool req = false ) {
    log( "File Properties"      , req );
    log( "\tFile Name"          , file.name             , req );
    log( "\tFile Size"          , file.size             , req );
    log( "\tFile Start"         , file.start            , req );
    log( "\tFile Current"       , file.current          , req );
    log( "\tFile Position"      , file.position         , req ); 
    log( "\tBlock Position"     , file.block_position   , req );
    log( "\tDisk Position"      , file.disk_position    , req );
    log( "\tFile End"           , file.end              , req );
    log( "\tMeta-data position" , file.metadata         , req ); 
}

void printBlock( os::Block &b , bool req = false ) {
    log( "Printing Block" , req );
    log( "\tBlock"        , b.block , req );
    log( "\tStatus"       , ( (b.status == os::FULL)?"FULL":"LAZY") , req );
    log( "\tPrevious"     , b.prev , req );
    log( "\tNext"         , b.next , req );
    log( "\tLength"       , b.length , req );
}

// Given a block and a start and end we remove all the bytes inbetween
void compact( os::Block &b, uint64_t start , uint64_t end ) {
    if( start >= end || start >= b.length ) return;

    // Remove trailing data
    if( end >= b.length ) {
        b.length = start;
        return;
    }


    uint64_t length = end - start;
    for( int i = 0 ; i < length; ++i ) {
        b.data[start + i] = b.data[end + i];
    }
    b.length -= length;
}


// namespace implementation

namespace os {

    // Private functions
    void FileSystem::insertFile( File &f ) {

        std::array<char,1024> buffer;

        // Length of name
        // name
        // disk usage
        // size
        // first block
        // last block
        // metadata
        
        uint64_t pos  = 0;
        uint64_t tmp= 0;
        char *buff = reinterpret_cast<char*>(&tmp);

        tmp = f.name.size();
        std::copy( buffer.data() + pos , buffer.data() + pos + sizeof(tmp) , buff );
        pos += sizeof(tmp);

        std::copy( buffer.data() + pos , buffer.data() + pos + tmp , f.name.begin() );
        pos += f.name.size();

        tmp = f.disk_usage;
        std::copy( buffer.data() + pos , buffer.data() + pos + sizeof(tmp) , buff );
        pos += sizeof(tmp);

        tmp = f.size;
        std::copy( buffer.data() + pos , buffer.data() + pos + sizeof(tmp) , buff );
        pos += sizeof(tmp);

        tmp = f.start;
        std::copy( buffer.data() + pos , buffer.data() + pos + sizeof(tmp) , buff );
        pos += sizeof(tmp);

        tmp = f.end;
        std::copy( buffer.data() + pos , buffer.data() + pos + sizeof(tmp) , buff );
        pos += sizeof(tmp);


        tmp = f.metadata;
        std::copy( buffer.data() + pos , buffer.data() + pos + sizeof(tmp) , buff );
        pos += sizeof(tmp);

        metaWriter->seek( 0 , END );
        metaWriter->write( pos , buffer.data() );

    }

    File& FileSystem::createNewFile( std::string name ) {
        enter( "CREATENEWFILE" );
        Block b = allocate( BlockSize , NULL );
        File &f = *(new File());
        f.name = name;
        f.size = 0;
        f.disk_usage = BlockSize;
        f.start = f.end = f.current = b.block;
        f.position = f.block_position = f.disk_position = 0;
        f.metadata = metadata->size;
        f.fs = this;

        insertFile( f );

        leave( "CREATENEWFILE" );
        return f;
    }

    void FileSystem::gotoBlock( uint64_t blockId ) {
        enter( "GOTOBLOCK" );
        uint64_t offset = blockId * TotalBlockSize;

        log( "Moving to block" , blockId );
        log( "At offset" , offset );

        stream.seekp( blockId * TotalBlockSize );
        stream.seekg( blockId * TotalBlockSize );
        leave( "GOTOBLOCK" );
    }

    Block FileSystem::readBlock( ) {
        enter( "READBLOCK" );
        Block ret;
        ret.status = FULL;
        ret.block = stream.tellp() / TotalBlockSize;

        log( "About to read block" , ret.block );
        assertStream( stream );

        lock( READ );
        {
            stream.read( reinterpret_cast<char*>( &ret.prev ) , sizeof( ret.prev ) );
            stream.read( reinterpret_cast<char*>( &ret.next ) , sizeof( ret.next ) );
            stream.read( reinterpret_cast<char*>( &ret.length ) , sizeof( ret.length ) );
            stream.read( ret.data.data() , ret.length );
        }
        unlock( READ );

        assertStream( stream );
        leave( "READBLOCK" );

        return ret;
    }

    void FileSystem::writeBlock( Block &b ) {
        enter( "WRITEBLOCK" );

        printBlock( b );

        assert( stream.tellp() == b.block * TotalBlockSize );

        assertStream( stream );

        lock( WRITE );
        {
            stream.write( reinterpret_cast<char*>( &(b.prev) ) , sizeof( b.prev ) );
            stream.write( reinterpret_cast<char*>( &(b.next) ) , sizeof( b.next ) );
            stream.write( reinterpret_cast<char*>( &(b.length) ) , sizeof( b.length ) );
            if( b.status == FULL ) {
                stream.write( b.data.data() , b.length );
            }
        }
        unlock( WRITE );

        assertStream( stream );
        leave( "WRITEBLOCK" );

    }

    void FileSystem::saveHeader() {
        enter( "SAVEHEADER" );

        assertStream( stream );

        log( "Writing Header to Disk" );
        log( "\tTotal number of bytes stored: "           , totalBytes );
        log( "\tFirst block of the free list: "           , freeList );
        log( "\tNumber of blocks used for file data: "    , numBlocks );
        log( "\tNumber of free blocks: "                  , numFreeBlocks );
        log( "\tNumber of files created: "                , numFiles );
        log( "\tAmount of data used for meta-data: "      , metadataSize );

        stream.seekp( SignatureSize , std::ios_base::beg );

        assert( stream.tellp() == 8 );

        // Write header
        stream.write( reinterpret_cast<char*>(&totalBytes) , sizeof(totalBytes) );
        stream.write( reinterpret_cast<char*>(&freeList) , sizeof(freeList) );
        stream.write( reinterpret_cast<char*>(&numBlocks) , sizeof(numBlocks) );
        stream.write( reinterpret_cast<char*>(&numFreeBlocks) , sizeof(numFreeBlocks) );
        stream.write( reinterpret_cast<char*>(&numFiles) , sizeof(numFiles) );
        stream.write( reinterpret_cast<char*>(&metadataSize) , sizeof(metadataSize) );
        stream.write( reinterpret_cast<char*>(&lastFileBlock) , sizeof(lastFileBlock) );
        stream.flush();

        assertStream( stream );
        leave( "SAVEHEADER" );

    }

    /*
     *  Locking Functions
     */

    // Lock the file for writing
    void FileSystem::lock( LockType type ){
        //enter( "LOCK" );
        switch( type ) {
            case READ:
                break;
            case WRITE:
                break;
        }
        //leave( "LOCK" );
    }

    void FileSystem::unlock( LockType type ) {
        //enter( "UNLOCK" );
        switch( type ) {
            case READ:
                break;
            case WRITE:
                break;
        }
        //leave( "UNLOCK" );
    }


    /*
     *  Low level filesystem functions
     */

    bool FileSystem::rename( File &toRename , const std::string newName ) {
        enter( "RENAME" );
        for( auto file = openFiles.begin() ; file != openFiles.end() ; ++file ) {
            File &f = *(*file);
            if( f.getFilename() == toRename.getFilename() ) {
                f.name = newName;
                toRename.name = newName;
                return true;
            }
        }
        leave( "RENAME" );
        return false;
    }


    void FileSystem::split( Block &b , uint64_t offset ) {
        enter( "SPLIT" );
        if( offset != b.length ) {

            Block newBlock = allocate( b.length - offset , b.data.begin() + offset );
            Block next = lazyLoad( b.next );

            newBlock.prev = b.block;
            newBlock.next = next.block;

            b.next = newBlock.block;
            next.prev = newBlock.block;

            b.length = offset;

            flush( newBlock );
            flush( b );
            flush( next );
        }
        leave( "SPLIT" );

    }

    //  grow    -   Grow the filesystem enough to support writing the number of bytes given
    //
    //  bytes   -   How much space we need
    //
    //  @return -   The first block of the new space
    //
    Block FileSystem::grow( uint64_t bytes , const char *buffer ) {
        enter( "GROW" );
        uint64_t blocksToWrite = (bytes + BlockSize - 1) / BlockSize;
        uint64_t current = numBlocks;

        const char Zero[BlockSize] = {0};
        if( buffer == NULL ) {
            buffer = Zero;
        }


        log( "Growing filesystem by" , bytes , true );
        log( "Blocks to be added" , blocksToWrite , true );

        // Just grow first
        lock( WRITE );
        {
            numBlocks += blocksToWrite;
            totalBytes += bytes;

            log( "Total Bytes" , numBlocks * TotalBlockSize , true );
            assertStream( stream );

            stream.seekp( numBlocks * TotalBlockSize , std::ios_base::beg );
            stream.write( Zero , 1 );
            stream.flush();

            saveHeader();
            assertStream( stream );
        }unlock( WRITE );

        assertStream( stream );

        // In case we have no data

        // Write new data
        stream.seekp( TotalBlockSize * current , std::ios_base::beg );

        uint64_t previous = numBlocks - 1;
        for( int i = 0 ; i < blocksToWrite - 1; ++i) {
            Block curr;
            curr.prev = previous;
            curr.block = current; 
            curr.next = current + 1;

            uint64_t bytesM = std::min( BlockSize , bytes );
            std::copy( buffer , buffer + BlockSize , curr.data.begin() );
            if( buffer != Zero ) buffer += BlockSize;
            writeBlock( curr );

            previous = current;
            ++current;
            bytes -= bytesM;
        }

        assert( bytes > 0 );

        Block curr;
        curr.status = FULL;
        curr.prev = (previous == current) ? 0 : previous;
        curr.block = current;
        curr.next = 0;
        curr.length = bytes;
        std::copy( buffer , buffer + bytes , curr.data.begin() );
        std::copy( Zero + bytes , Zero + BlockSize , curr.data.begin() + bytes );

        printBlock( curr );
        flush( curr );

        curr = load( numBlocks - blocksToWrite );

        assert( ( numBlocks - blocksToWrite - 1) < 1000 );
        leave( "GROW" );

        return curr;
    }


    //  load   -   Given a block id we load it from disk if not already loaded
    // 
    //  block       -   Block id we want to load
    //
    //  @return     -   Loaded block
    //
    Block FileSystem::load( uint64_t block ) {
        enter( "LOAD" );
        assert( stream.bad() == false );
        assert( stream.fail() == false );
        assert( block < 10 );
        assert( block > 0 );

        gotoBlock( block );
        Block b = readBlock();

        leave( "LOAD" );

        return b;
    }

    //  lazyLoad   - Given a block id only load meta-data
    //  
    //  block           - The block we wish to load
    //
    //  @return         - The loaded block
    //
    // TODO: Caching
    //
    Block FileSystem::lazyLoad( uint64_t block ) {
        enter( "LAZYLOAD" );
        Block b;
        b.status = LAZY;
        b.block = block;
        gotoBlock( block );
        lock( READ );
        {
            stream.read( reinterpret_cast<char*>( &b.prev ) , sizeof(b.prev) );
            stream.read( reinterpret_cast<char*>( &b.next ) , sizeof(b.next) );
            stream.read( reinterpret_cast<char*>( &b.length ) , sizeof(b.length) );
        }
        unlock( READ );

        leave( "LAZYLOAD" );

        return b;
    }

    //  flush   -   Given a block we flush it to disk
    //
    //  b       -   The block, with its data, we want to save to disk
    //  
    //
    void FileSystem::flush( Block &b ) {
        enter( "FLUSH" );
        gotoBlock( b.block );
        writeBlock( b );
        leave( "FLUSH" );
    }

    //  reuse   -   Given some amount of bytes and data we try to load blocks from free list
    //
    //      length and buffer is modified as we use data.  If not enough free blocks exist
    //      then length is the number of bytes that we could not store and buffer points 
    //      to the first element which we could not store.
    //
    //  length  -   Number of bytes of data
    //  buffer  -   data
    //
    //  @return -   The first block in the freelist
    Block FileSystem::reuse( uint64_t &length , const char* &buffer ) {
        enter( "REUSE" );
        Block head;

        assertStream( stream );

        if( numFreeBlocks > 0 ) {
            uint64_t prevBlock = 0,currBlock = freeList;
            Block current;

            head = lazyLoad( currBlock );

            do {
                // Update free list
                freeList = current.next;
                --numFreeBlocks;

                // Currently we treat the freeList as a special file, so just use it as such
                current = lazyLoad( currBlock );
                current.status = FULL;
                current.length = std::min( length , BlockSize );
                uint64_t fillOffset = 0;
                if( buffer != 0 ) {
                    std::copy( buffer , buffer + current.length , current.data.data() );
                    fillOffset = BlockSize - current.length;
                }
                std::fill( current.data.begin() + fillOffset , current.data.begin() + BlockSize , 0 );

                // Update input
                buffer += current.length;
                length -= current.length;

                // Write to disk
                flush( current );

                // Go to next
                prevBlock = currBlock;
                currBlock = freeList;

            } while ( numFreeBlocks > 0 && length > 0 );

            current.next = 0;
            head.prev = prevBlock;
            flush( current );
            flush( head );
        }

        leave( "REUSE" );
        return head;
    }

    //  allocate    -   Allocate enough bytes to support writing the data given
    //
    //  length      -   The amount of data in bytes
    //  buffer      -   The data
    //
    //  @return     -   The first block which holds our data
    //
    Block FileSystem::allocate( uint64_t length , const char *buffer ) {
        enter( "ALLOCATE" );
        assertStream( stream );

        Block head;
        if( length == 0 ) {
            assert( length != 0 );
            return head;
        }

        if( numFreeBlocks > 0 ){
            assert( numFreeBlocks == 0 );
            Block head = reuse( length , buffer );
            if( length > 0 ) {
                Block grown = grow( length , buffer );

                // The very end
                uint64_t tmp = grown.prev;

                // Join in middle
                Block b_tmp = lazyLoad( head.prev );
                b_tmp.next = grown.block;
                grown.prev = head.prev;

                // Point to actual end
                head.prev = tmp;

            }
        }else {
            head = grow( length , buffer );
        }
        leave( "ALLOCATE" );
        return head;
    }


    //  locate    -   Given a block id and an offset find the block which
    //                  has the data corresponding to that offset.
    //
    //  start       -   The starting block
    //  offset      -   How many bytes from the starting block, this is updated
    //
    //  @return     -   The block which holds the data pointed at by the offset
    //
    Block FileSystem::locate( uint64_t start , uint64_t &offset ) {
        enter( "LOCATE" );

        Block b = lazyLoad( start );
        while( offset >= b.length ) {
            offset -= b.length;
            if( b.next == 0 ) {
                b = Block();
                break;
            }
            b = lazyLoad( b.next );
        }
        leave( "LOCATE" );
        return b;
    }

    //  read    -   Read some given number of bytes starting at the offset and 
    //              store it in the buffer provided
    //
    //  start   -   Starting block to measure offset from
    //  offset  -   Offset from starting block in bytes
    //  length  -   How many bytes to try to read
    //  buffer  -   Where to store the data
    //
    //  @return -   Number of bytes read
    uint64_t FileSystem::read( File &file , uint64_t length , char* buffer ) {
        enter( "READ" );

        printFile( file );

        assertStream( stream );

        uint64_t requested = length;
        length = std::min( length , file.size - file.position );

        if( length > 0 && buffer != 0 && file.position < file.size ) {
            Block current = locate( file.current , file.position );
            assert( file.position < 1000 );

            printBlock( current );

            uint64_t next = current.block;
            current = load( next );

            printBlock( current );

            for( int i = 0 ; i < length ; ++i ) {
                buffer[i] = current.data[file.position];
                ++file.position;
                if( file.position == current.length ) {
                    if( current.next != 0 ) {
                        current = load( current.next );
                        file.current = current.block;
                        file.position = 0;
                    }
                }
            }
        }else {
            assert( false );
        }
        // How much we read

        assertStream( stream );
        leave( "READ" );

        return requested - length;
    }

    //  write   -   Given some data write it at an offset from a starting block
    //
    //  start   -   The starting block
    //  offset  -   The nunber of bytes to our place we wish to write to
    //  length  -   How much data we want to write
    //  buffer  -   The data we want to write
    //
    //      If the buffer is NULL we write the NULL character.
    //
    //  @return -   How much data we wrote
    uint64_t FileSystem::write( File &file , uint64_t length , const char* buffer ) {
        enter( "WRITE" );

        // Going to fill up rest of the file
        if( length + file.disk_position > file.disk_usage ) {
            assert( false );
            // Calculate how much extra space we need
            uint64_t growBy = length - (file.disk_usage - file.disk_position);

            // Load for reconnection
            Block oldBlock = lazyLoad( file.end );
            Block newBlock = grow( growBy , NULL );

            // Update/Reconnect
            file.disk_usage += roundTo<BlockSize>( growBy );
            file.end = newBlock.prev;
            oldBlock.next = newBlock.block;
            newBlock.prev = oldBlock.block;
            flush( newBlock );
            flush( oldBlock );

            // Save file data
            file.size += growBy; 
        }

        // We assume we always have space to write to

        uint64_t curr = file.current;
        uint64_t written = 0;
        uint64_t overwritten = 0;

        // While we have not written everything
        while( written < length ) {

            // Go to the current block
            file.current = curr;
            Block b = load( curr );
            assert( b.length == 0 );

            // How much room is there to write our data
            uint64_t len = std::min( BlockSize - file.block_position , length );
            // How much data will we actually overwrite
            overwritten = file.block_position - b.length;

            std::copy( buffer , buffer + len , b.data.data() + b.length );
            b.length = file.block_position + len;
            flush( b );

            written += len;

            file.position += len;
            file.block_position = b.length % BlockSize;

            curr = b.next;
        }

        // Need to move blocks around
        if( overwritten < length ) {
            assert( false && "Handle overwritten < length"  );
        }

        log( "Overwritten" , overwritten , true );

        // TODO: What happens if we fill up file exactly to end?

        //file.flush();

        leave( "WRITE" );
        return written;
    }

    //  insert  -   We insert data at a given offset in the file.  We do not write
    //              over any data.
    //
    //  start   - Starting block to begin search from
    //  offset  -   How many bytes to skip over
    //  length  -   The amount of data to insert
    //  buffer  -   The data to insert
    //
    //      If the buffer is NULL we write the null character
    //
    //  @return -   The number of bytes written
    uint64_t FileSystem::insert( File &file , uint64_t length , const char* buffer ) {
        enter( "INSERT" );
        assert( numFreeBlocks == 0 );
        if( length > 0 ) {
            // Grow file if neccessary 
            if( file.position > file.size ) {
                // Actually allocate
                Block remaining = allocate( file.position - file.size , NULL );

                // Grab the current end of file block
                Block oldEnd = lazyLoad( file.end );

                // Update file
                file.end = remaining.prev;
                file.size += file.position - file.size;
                file.current = file.end;

                // Merge blocks together
                remaining.prev = oldEnd.block;
                oldEnd.next = remaining.block;

                flush( remaining );
                flush( oldEnd );
            }

            Block b = locate( file.current , file.position );

            // We might have to split
            split( b , file.position % BlockSize );

            // Now append to here
            Block next = allocate( length , buffer );
            Block end = lazyLoad( next.prev );
            end.next = b.next;
            next.prev = b.block;
            b.next = next.block;

            flush( next );
            flush( end );
            flush( b );

            // file.flush();
        }
        leave( "INSERT" );
        return length;
    }

    //
    //  releaseBlock    -   Adds a block to the freelist for later use
    //
    //  block           -   The block we are freeing
    //
    void releaseBlock( uint64_t block ) {
    }

    //
    //  remove  -   Remove bytes from a file
    //
    //  file    -   The file we want to remove bytes from
    //  length  -   How many bytes to remove
    //
    //  @return -   How many bytes actually removed
    //
    uint64_t FileSystem::remove( File &file ,uint64_t length ) {
        enter( "REMOVE" );
        assert( numFreeBlocks == 0 );
        uint64_t remaining = std::min( length , file.size - file.position );

        // Make sure can remove bytes
        if( remaining > 0 && (file.size > file.position) ) {

            // Load first block we have to modify
            Block first = locate( file.start , file.position );

            // Calculate how much to remove from first
            uint64_t removeFromFirst = std::min( remaining , first.length - file.position );
            // and how many bytes to skip to get to new spot
            remaining -= removeFromFirst;

            // Load final block
            Block last = locate( first.next , remaining );
            last = load( last.block );

            // Compact blocks
            compact( first , file.current , removeFromFirst );
            compact( last , 0 , remaining );

            // Need to reconnect
            if( first.next != last.block ) {
                releaseBlock( first.next );
                first.next = last.block;
                last.prev = first.block;
            }

            // Update file
            file.size -= length - remaining;
            file.current = last.block;
            file.position = 0;

            // Write to disk
            flush( first );
            flush( last );

            // file.flush();
        }
        leave( "REMOVE" );
        return length - remaining;

    }


    // Public Methods/Functions

    void FileSystem::shutdown() {
        enter( "SHUTDOWN" );
        for( auto file = openFiles.begin() ; file != openFiles.end() ; ) {
            //(*file).close();
            openFiles.erase(file);
        }
        leave( "SHUTDOWN" );
        stream.close();
    }

    std::list<File*> FileSystem::getFiles() {
        enter( "GETFILES" );
        std::list<File*> ret;
        for( auto file = allFiles.begin() ; file != allFiles.end() ; ++file ) {
            ret.push_back(*file);
        }
        leave( "GETFILES" );
        return ret;
    }

    std::list<File*> FileSystem::getOpenFiles() {
        enter( "GETOPENFILES" );
        std::list<File*> ret;
        for( auto file = openFiles.begin() ; file != openFiles.end() ; ++file ) {
            ret.push_back(*file);
        }
        return ret;
        leave( "GETOPENFILES" );
    }

    std::list<std::string> FileSystem::getFilenames() {
        enter( "GETFILENAMES" );
        std::list<std::string> names;
        for( auto file = allFiles.begin() ; file != allFiles.end() ; ++file ) {
            names.push_back( (*file)->getFilename() );
        } 
        leave( "GETFILENAMES" );
        return names;
    }

    File& FileSystem::open( const std::string name) {
        enter( "OPEN" );
        for( auto file = allFiles.begin() ; file != allFiles.end() ; ++file ) {
            if( (*file)->getFilename() == name ) {
                return **file;
                leave("OPEN");
            }
        } 

        // Create new file
        return createNewFile( name );
        leave( "OPEN" );
    }

    bool FileSystem::unlink( File &f ) {
        enter( "UNLINK:1" );
        leave( "UNLINK:1" );
    }

    //
    bool FileSystem::unlink( const std::string filename ) {
        enter( "UNLINK:2" );
        File& file = open(filename);
        leave( "UNLINK:2" );
        return file.unlink();
    }

    bool FileSystem::exists( const std::string filename ) {
        enter( "EXISTS" );
        File& file = open(filename);
        leave( "EXISTS" );
        return file.getStatus() == OPEN;
    }


    FileSystem::FileSystem( const std::string filename) {
        enter( "FILESYSTEM" );
        std::array<char,1024> buff;

        fileSystemLocation = filename;

        //  Open the file
        stream.open( filename , std::fstream::in |  std::fstream::out | std::fstream::binary );

        if( !stream ) {
            stream.open( filename , std::fstream::binary | std::fstream::trunc | std::fstream::out );
            stream.close();
            stream.open( filename , std::fstream::in |  std::fstream::out | std::fstream::binary );

            log( "The file does not exist, so we are creating it" );
            totalBytes = 0;
            freeList = 0;
            numBlocks = 2;
            numFreeBlocks = 0;
            numFiles = 0;
            metadataSize = 0;
            lastFileBlock = 1;

            // Now we go to the beginning
            stream.seekp( 0 , std::ios_base::beg );

            // Write header
            stream.write( HeaderSignature.data() , SignatureSize );
            stream.write( reinterpret_cast<char*>(&totalBytes) , sizeof(totalBytes) );
            stream.write( reinterpret_cast<char*>(&freeList) , sizeof(freeList) );
            stream.write( reinterpret_cast<char*>(&numBlocks) , sizeof(numBlocks) );
            stream.write( reinterpret_cast<char*>(&numFreeBlocks) , sizeof(numFreeBlocks) );
            stream.write( reinterpret_cast<char*>(&numFiles) , sizeof(numFiles) );
            stream.write( reinterpret_cast<char*>(&metadataSize) , sizeof(metadataSize) );
            stream.write( reinterpret_cast<char*>(&lastFileBlock) , sizeof(lastFileBlock) );

            assert( TotalBlockSize == 1024 );
            stream.seekp( TotalBlockSize , std::ios_base::beg );

            // Write file "data-strcture"
            stream.write( reinterpret_cast<char*>(&totalBytes) , sizeof(totalBytes) ); // Prev
            stream.write( reinterpret_cast<char*>(&totalBytes) , sizeof(totalBytes) );  // Next
            stream.seekp( TotalBlockSize * 2 - 1 );
            stream.write( reinterpret_cast<char*>(&totalBytes) , sizeof( totalBytes ));

            stream.flush();

        }else {

            //  Read the header
            stream.read( buff.data() , SignatureSize );

            if( std::strncmp( buff.data() , HeaderSignature.data() , SignatureSize ) != 0 ) {
                std::cout << "Invalid header signature found" << std::endl;
                std::exit( -1 );
            }

            stream.read( reinterpret_cast<char*>(&totalBytes)       , sizeof(totalBytes) );
            stream.read( reinterpret_cast<char*>(&freeList)         , sizeof(freeList) );
            stream.read( reinterpret_cast<char*>(&numBlocks)        , sizeof(numBlocks) );
            stream.read( reinterpret_cast<char*>(&numFreeBlocks)    , sizeof(numFreeBlocks) );
            stream.read( reinterpret_cast<char*>(&numFiles)         , sizeof(numFiles) );
            stream.read( reinterpret_cast<char*>(&metadataSize)     , sizeof(metadataSize) );
            stream.read( reinterpret_cast<char*>(&lastFileBlock)     , sizeof(lastFileBlock) );

        }

        log( "Total number of bytes stored"           , totalBytes );
        log( "First block of the free list"           , freeList );
        log( "Number of blocks used for file data"    , numBlocks );
        log( "Number of free blocks"                  , numFreeBlocks );
        log( "Number of files created"                , numFiles );
        log( "Amount of data used for meta-data"      , metadataSize );

        //  Read the filenames
        log( "Loading File Meta-data" );
        log( "Number of files" , numFiles );

        metadata = new File();

        metadata->fs = this;
        metadata->name = "Metadata";
        metadata->start = 1;
        metadata->current = 1;
        metadata->end = lastFileBlock;
        metadata->size = metadataSize;

        metaWriter = new FileWriter( *metadata );
        metaReader = new FileReader( *metadata );

        printFile( *metadata , true );

        log( "Files to read" , numFiles , true );

        for( int i = 0 ; i < numFiles; ++i) {
            File &f = *(new File());

            uint64_t str_len,f_position = metaReader->tell(); 
            metaReader->read( sizeof(uint64_t) , reinterpret_cast<char*>(&str_len) );
            metaReader->read( sizeof(uint64_t) , reinterpret_cast<char*>(&str_len) );
            metaReader->read( str_len , buff.data() );
            metaReader->read( sizeof(uint64_t) , reinterpret_cast<char*>(&(f.start)) );
            metaReader->read( sizeof(uint64_t) , reinterpret_cast<char*>(&(f.end)) );
            metaReader->read( sizeof(uint64_t) , reinterpret_cast<char*>(&(f.size)) );

            f.fs = this;
            f.metadata = metadata->position;
            f.name = std::string( buff.data() , str_len );
            f.current = f.start;

            allFiles.push_back( &f );
        }

        log( "Finished reading Meta-data" );

        leave( "FILESYSTEM" );
        assertStream( stream );
    }

    void FileSystem::closing( File& fp ) {
        enter( "CLOSING" );
        for( auto file = openFiles.begin(); file != openFiles.end(); ++file ) {
            File &f = *(*file);
            if( f.getFilename() == fp.getFilename() ) {
                openFiles.erase(file);
                break;
            }
        }
        leave( "CLOSING" );
    }

    FileSystem::~FileSystem() {
        enter( "~FILESYSTEM" );
        shutdown();
        delete metaWriter;
        delete metaReader;
        leave( "~FILESYSTEM" );
    }

};
