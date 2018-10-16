#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D64_SIZE 174848
typedef unsigned char u8;

static const int TrackOffsets[] = {
	0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400, 0x06900, 0x07E00,
	0x09300, 0x0A800, 0x0BD00, 0x0D200, 0x0E700, 0x0FC00, 0x11100,
	0x12600, 0x13B00, 0x15000, 0x16500, 0x17800, 0x18B00, 0x19E00,
	0x1B100, 0x1C400, 0x1D700, 0x1EA00, 0x1FC00, 0x20E00, 0x22000,
	0x23200, 0x24400, 0x25600, 0x26700, 0x27800, 0x28900, 0x29A00,
	0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00
};

static const char* FileTypes[] = {
	"DEL", "SEQ", "PRG", "USR", "REL", "???", "???", "???"
};

int d64Sector( u8 track, u8 sector )
{
	return TrackOffsets[ track-1 ] + 256 * (int)sector;
}


int fileCount( u8* d64 )
{
	int numFiles = 0;
	u8* curr = d64 + d64Sector( 18, 0 );
	while(curr[0]) {
		curr = d64 + d64Sector( curr[0], curr[1] );
		u8* file = curr;
		while( ( file - curr ) < 256 ) {
			u8 type = file[ 2 ];	// type == 0 => "deleted"
			if( type ) {
/*				char name[17];
				for( int n=0; n<16; ++n ) { name[n] = file[n + 5] == 0xa0 ? '.' : file[n+5]; }
				name[ 16 ] = 0;
				printf( "file %03d: \"%s\" %s\n", numFiles, name, FileTypes[type&7] );
*/				++numFiles;
			}
			file += 32;
		}
	}
	return numFiles;
}

u8* d64Merge( u8* data, u8* dir )
{
	int files_in_data = fileCount(data);
	int files_in_dir = fileCount(dir);

	printf("files in data d64: %d\nfiles in dir d64: %d\n", files_in_data, files_in_dir );

	if( files_in_data > files_in_dir ) {
		printf("Error: files in data d64 exceeds the number of files in the dir art d64.\n");
		return 0;
	}

	// allocate a d64 for the output
	u8* out = (u8*)malloc( D64_SIZE );
	if( out == 0 ) { return out; }

	// all the data on the disk comes from the data d64
	memcpy( out, data, D64_SIZE );

	// the folder structure comes from the dir d64
	// copy track 18 from dir to out
	memcpy( out + d64Sector( 18, 1 ), dir + d64Sector( 18, 1), d64Sector( 19, 0 ) - d64Sector( 18, 1 ) );

	// copy file type, disk location, file name prefix from the data disk
	int currFile = 0;
	u8* data_curr = data + d64Sector( 18, 0 );
	u8* data_file = 0;
	u8* dir_curr = dir + d64Sector( 18, 0 );
	u8* dir_file = 0;

	// only update files from the data directory
	while( currFile < files_in_data ) {
		for(;;) { // grab the next data file
			if( !data_file || ( data_file - data_curr ) >= 0x100 ) {
				data_curr = data + d64Sector( data_curr[ 0 ], data_curr[ 1 ] );
				data_file = data_curr;
				break;
			}
			data_file += 32;
			if( (data_file - data_curr ) < 0x100 && data_file[2] ) { break; }
		}
		for(;;) { // grab the next directory file
			if( !dir_file || ( dir_file - dir_curr ) >= 0x100 ) {
				dir_curr = dir + d64Sector( dir_curr[ 0 ], dir_curr[ 1 ] );
				dir_file = dir_curr;
				break;
			}
			dir_file += 32;
			if( ( dir_file - dir_curr ) < 0x100 && dir_file[ 2 ] ) { break; }
		}
		// the out file is relative to the dir d64
		u8* out_file = out + ( dir_file - dir );

		// copy the file info from the data file into the out file
		memcpy( out_file+2, data_file+2, 3 );
		memcpy( out_file + 21, data_file + 21, 11 );

		// first filename entirely from dir art disk
		if(currFile) {
			for(int i = 5; i<21 && data_file[i] != 0xa0; ++i ) { out_file[i] = data_file[i]; }
		}
		++currFile;
	}
	return out;
}

void* load(const char* filename, size_t* size_ret)
{
	FILE* f = fopen( filename, "rb" );
	if( f ) {
		fseek( f, 0, SEEK_END );
		size_t size = ftell( f );
		fseek( f, 0, SEEK_SET );
		void* buf = malloc( size );
		if( buf ) {
			fread( buf, size, 1, f );
			if( size_ret ) { *size_ret = size; }
		}
		fclose( f );
		return buf;
	}
	return 0;
}

int save( void* buf, const char* filename, size_t size )
{
	FILE* f = fopen( filename, "wb" );
	if( f ) {
		fwrite( buf, size, 1, f );
		fclose( f );
		return 0;
	}
	return 1;
}

int main( int argc, char* argv[] )
{
	if( argc <= 3 )
	{
		printf("usage:\n%s <d64 data> <d64 dir> <d64 out>\n", argv[0] );
		printf("Notes:\n* .d64 files all need different names\n* .d64 dir should have more files than d64 data.\n");
		return 0;
	}

	const char* data = argv[1];
	const char* dir = argv[2];
	const char* out = argv[3];


	printf("Files names from \"%s\" will be padded with corresponding file names from\n \"%s\"\n and saved out as a new disk named\n \"%s\".\n",
			data, dir, out );

	size_t data_size, dir_size;

	u8* data_d64 = (u8*)load( data, &data_size );
	u8* dir_d64 = ( u8* )load( dir, &dir_size );
	u8* out_d64 = 0;

	int ret = 0;

	if( data_d64 && dir_d64 && data_size == D64_SIZE && dir_size == D64_SIZE )
	{
		out_d64 = d64Merge( data_d64, dir_d64 );
		if( !out_d64 ) { ret = 1; }
		else { ret = save( out_d64, out, D64_SIZE ); }
	}

	if( data_d64 ) { free(data_d64); }
	if( dir_d64 ) { free(dir_d64); }
	if( out_d64 ) { free(out_d64); }

	return out_d64 ? 0 : 1;
}