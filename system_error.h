#ifndef SYSTEM_ERROR_H
#define SYSTEM_ERROR_H

// ==================================================
// Error Types
// ==================================================

enum ErrorType
{
    ERROR_NONE = 0,
    ERROR_CONNECTION,
    ERROR_VOLTAGE_LOW,
    ERROR_VOLTAGE_HIGH,
    ERROR_CURRENT_LOW,
    ERROR_CURRENT_HIGH
};

// Read-only access to the current system error.
ErrorType tasks_get_error_type(void);

#endif
