/****************************************************************************
 * VLCLibraryFixtureSQL.m: isolated SQL mutations for medialibrary tests
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *****************************************************************************/

#import <Foundation/Foundation.h>

#include <ctype.h>
#include <sqlite3.h>
#include <stdlib.h>

// Match medialibrary's FILENAME collation, including leading-number comparison.
static int CompareFilenames(void * __unused context, int lhsSize, const void *lhs,
                            int rhsSize, const void *rhs)
{
    const char *left = lhs;
    const char *right = rhs;
    int size = MIN(lhsSize, rhsSize);
    if (size == 0)
        return lhsSize - rhsSize;
    if (!isdigit((unsigned char)*left) || !isdigit((unsigned char)*right))
        return sqlite3_strnicmp(left, right, size);

    int leftNumber = 0;
    while (lhsSize > 0 && isdigit((unsigned char)*left)) {
        leftNumber = leftNumber * 10 + (*left++ - '0');
        lhsSize--;
    }
    int rightNumber = 0;
    while (rhsSize > 0 && isdigit((unsigned char)*right)) {
        rightNumber = rightNumber * 10 + (*right++ - '0');
        rhsSize--;
    }
    if (leftNumber != rightNumber)
        return leftNumber - rightNumber;
    return sqlite3_strnicmp(left, right, MIN(lhsSize, rhsSize));
}

static BOOL ExecuteStatement(sqlite3 *database, NSDictionary *request, NSString **error)
{
    NSString * const sql = request[@"sql"];
    NSArray * const parameters = request[@"parameters"];
    NSNumber * const minimumChanges = request[@"minimumChanges"];
    if (![sql isKindOfClass:NSString.class] ||
        ![parameters isKindOfClass:NSArray.class] ||
        ![minimumChanges isKindOfClass:NSNumber.class]) {
        *error = @"Invalid fixture SQL statement request";
        return NO;
    }
    sqlite3_stmt *statement = NULL;
    BOOL success = sqlite3_prepare_v2(database, sql.UTF8String, -1, &statement, NULL) == SQLITE_OK;
    if (success && (NSUInteger)sqlite3_bind_parameter_count(statement) != parameters.count) {
        *error = @"Fixture SQL parameter count does not match the statement";
        success = NO;
    }
    for (NSUInteger index = 0; success && index < parameters.count; index++) {
        id const value = parameters[index];
        int result;
        if ([value isKindOfClass:NSNumber.class])
            result = sqlite3_bind_int64(statement, (int)index + 1, [value longLongValue]);
        else if ([value isKindOfClass:NSString.class])
            result = sqlite3_bind_text(statement, (int)index + 1, [value UTF8String], -1, SQLITE_TRANSIENT);
        else if (value == NSNull.null)
            result = sqlite3_bind_null(statement, (int)index + 1);
        else {
            *error = @"Unsupported fixture SQL parameter type";
            success = NO;
            break;
        }
        success = result == SQLITE_OK;
    }
    if (success)
        success = sqlite3_step(statement) == SQLITE_DONE;
    if (success && sqlite3_changes(database) < minimumChanges.longLongValue) {
        *error = [NSString stringWithFormat:@"Fixture SQL changed %d rows, expected at least %@",
                  sqlite3_changes(database), minimumChanges];
        success = NO;
    }
    if (!success && *error == nil)
        *error = [NSString stringWithUTF8String:sqlite3_errmsg(database)];
    sqlite3_finalize(statement);
    return success;
}

static BOOL ExecuteRequest(NSDictionary *request, NSString **error)
{
    NSString * const path = request[@"database"];
    NSArray * const statements = request[@"statements"];
    if (![path isKindOfClass:NSString.class] || path.length == 0 ||
        ![statements isKindOfClass:NSArray.class] || statements.count == 0) {
        *error = @"Invalid fixture SQL request";
        return NO;
    }
    sqlite3 *database = NULL;
    BOOL success = sqlite3_open_v2(path.fileSystemRepresentation, &database,
                                 SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK;
    if (success) {
        success = sqlite3_busy_timeout(database, 5000) == SQLITE_OK &&
            sqlite3_create_collation_v2(database, "FILENAME", SQLITE_UTF8, NULL,
                                        CompareFilenames, NULL) == SQLITE_OK &&
            sqlite3_exec(database, "PRAGMA foreign_keys = ON", NULL, NULL, NULL) == SQLITE_OK &&
            sqlite3_exec(database, "PRAGMA recursive_triggers = ON", NULL, NULL, NULL) == SQLITE_OK &&
            sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK;
    }
    for (NSUInteger index = 0; success && index < statements.count; index++) {
        id const statement = statements[index];
        if (![statement isKindOfClass:NSDictionary.class]) {
            *error = @"Invalid fixture SQL statement request";
            success = NO;
        } else {
            success = ExecuteStatement(database, statement, error);
        }
        if (!success)
            *error = [NSString stringWithFormat:@"Statement %lu: %@", (unsigned long)index + 1, *error];
    }
    if (success)
        success = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL) == SQLITE_OK;
    if (!success) {
        if (*error == nil)
            *error = database == NULL ? @"Could not open fixture database" :
                [NSString stringWithUTF8String:sqlite3_errmsg(database)];
        if (database != NULL)
            sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    }
    if (database != NULL) {
        const int result = sqlite3_close(database);
        if (success && result != SQLITE_OK) {
            *error = @"Could not close fixture database";
            success = NO;
        }
    }
    return success;
}

int main(void)
{
    @autoreleasepool {
        NSError *parseError = nil;
        NSData * const data = [NSFileHandle.fileHandleWithStandardInput readDataToEndOfFile];
        id const request = [NSJSONSerialization JSONObjectWithData:data options:0 error:&parseError];
        NSString *error = nil;
        BOOL success = NO;
        if ([request isKindOfClass:NSDictionary.class])
            success = ExecuteRequest(request, &error);
        else
            error = parseError.localizedDescription ?: @"Invalid fixture SQL request";
        NSDictionary * const response = success ? @{ @"success": @YES } :
            @{ @"success": @NO, @"error": error ?: @"Fixture SQL operation failed" };
        NSData * const responseData = [NSJSONSerialization dataWithJSONObject:response options:0 error:NULL];
        [NSFileHandle.fileHandleWithStandardOutput writeData:responseData];
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
