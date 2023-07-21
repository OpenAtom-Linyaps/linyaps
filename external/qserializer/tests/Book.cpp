#include "Book.h"

#include <QSerializer/QSerializer.h> // for QSERIALIZER_IMPL

#if QT_VERSION <= QT_VERSION_CHECK(6, 2, 0)
#include "custom_converter.h" // for registerQListQStringConveter
#endif

QSERIALIZER_IMPL(Book

#if QT_VERSION <= QT_VERSION_CHECK(6, 2, 0)
                 ,
                 { registerQListQStringConveter(); }
#endif

);
