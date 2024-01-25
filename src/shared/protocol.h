#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <linux/types.h>

#define ROOT_DIR_INODE_N 1337
#define MAX_NAME_SIZE 256
#define MAX_OBJECTS_COUNT 32
#define MAX_DATA_LENGTH 1024


typedef struct Data
{
    int32_t length;
    char data[MAX_DATA_LENGTH];
} Data;



typedef enum ObjectType
{
    OBJECT_TYPE_FILE = 1,
    OBJECT_TYPE_DIR,
} ObjectType;

typedef struct ObjectInfo
{
    ObjectType type;
    unsigned long inode_n;
} ObjectInfo;

typedef struct Object
{
    ObjectInfo info;
    char name[MAX_NAME_SIZE];
} Object;

typedef struct Objects
{
    uint16_t count;
    Object objects[MAX_OBJECTS_COUNT];
} Objects;


typedef enum MethodType
{
    METHOD_TYPE_CREATE = 1,
    METHOD_TYPE_LINK,
    METHOD_TYPE_UNLINK,
    METHOD_TYPE_READ,
    METHOD_TYPE_WRITE,
    METHOD_TYPE_LIST,
    METHOD_TYPE_RMDIR,
    METHOD_TYPE_LOOKUP,
    METHOD_TYPE_MOUNT,
} MethodType;


typedef enum MethodStatus
{
    METHOD_STATUS_OK = 1,
    METHOD_STATUS_ERR,
} MethodStatus;


typedef struct MountRequest {} MountRequest;

typedef struct MountResponse
{
    unsigned long inode_n;
} MountResponse;


typedef struct CreateRequest
{
    ObjectType type;
    unsigned long parent_inode_n;
    char name[MAX_NAME_SIZE];
} CreateRequest;

typedef struct CreateResponse
{
    unsigned long inode_n;
} CreateResponse;


typedef struct LinkRequest
{
    unsigned long source_inode_n;
    unsigned long parent_inode_n;
    char name[MAX_NAME_SIZE];
} LinkRequest;

typedef struct LinkResponse {} LinkResponse;


typedef struct UnlinkRequest
{
    unsigned long parent_inode_n;
    char name[MAX_NAME_SIZE];
} UnlinkRequest;

typedef struct UnlinkResponse {} UnlinkResponse;


typedef struct ReadRequest
{
    unsigned long inode_n;
} ReadRequest;

typedef struct ReadResponse
{
    Data data;
} ReadResponse;


typedef struct WriteRequest
{
    unsigned long inode_n;
    Data data;
} WriteRequest;

typedef struct WriteResponse {} WriteResponse;


typedef struct ListRequest
{
    unsigned long inode_n;
} ListRequest;

typedef struct ListResponse
{
    Objects objects;
} ListResponse;


typedef struct RmdirRequest
{
    unsigned long parent_inode_n;
    char name[MAX_NAME_SIZE];
} RmdirRequest;

typedef struct RmdirResponse {} RmdirResponse;


typedef struct LookupRequest
{
    unsigned long parent_inode_n;
    char name[MAX_NAME_SIZE];
} LookupRequest;

typedef struct LookupResponse
{
    ObjectInfo info;
} LookupResponse;


typedef struct MethodRequest
{
    MethodType type;
    union
    {
        CreateRequest create;
        LinkRequest link;
        UnlinkRequest unlink;
        ReadRequest read;
        WriteRequest write;
        ListRequest list;
        RmdirRequest rmdir;
        LookupRequest lookup;
        MountRequest mount;
    };
} MethodRequest;


typedef struct MethodResponse
{
    MethodStatus status;
    MethodType type;
    union
    {
        CreateResponse create;
        LinkResponse link;
        UnlinkResponse unlink;
        ReadResponse read;
        WriteResponse write;
        ListResponse list;
        RmdirResponse rmdir;
        LookupResponse lookup;
        MountResponse mount;
    };
} MethodResponse;



#endif