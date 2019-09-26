#pragma once

// defines bitmap in system memory
class PixelsArray final: public cxx::noncopyable
{
public:
    eTextureFormat mFormat = eTextureFormat_Null;

    int mSizex = 0;
    int mSizey = 0;
    unsigned char* mData = nullptr;

public:
    PixelsArray() = default;
    ~PixelsArray();

    // Allocates array of pixels of specified format and dimensions
    // @param format: Format
    // @param sizex, sizey: Dimensions
    bool Create(eTextureFormat format, int sizex, int sizey);
    // Free allocated memory
    void Cleanup();

    // Load bitmap from external file
    // Note that filesystem must be initialized otherwise error occurs
    // @param fileName: File name
    // @param forceFormat: Explicit format conversion, optional
    bool LoadFromFile(const char* fileName, eTextureFormat forceFormat = eTextureFormat_Null);

    // Save bitmap content to external file
    // Note that filesystem must be initialized otherwise error occurs
    // @param fileName: File name
    bool SaveToFile(const char* fileName);

    // Fill bitmap with checkerboard pattern, does not allocate memory
    // @returns false if bitmap null
    bool FillWithCheckerBoard();

    // Fill bitmap with solid color, does not allocate memory
    // @returns false if bitmap null
    bool FillWithColor(Color32 color);

    // Test whether bitmap has pixels
    bool HasContent() const;

    // Save pixels of specified format to external file
    // @param fileName: File name
    // @param format: Format of pixels array
    // @param sizex, sizey: Picture dimensions
    // @param pixels: Source data
    static bool SaveToFile(const char* fileName, eTextureFormat format, int sizex, int sizey, unsigned char* pixels);
};