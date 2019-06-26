//----------------------------------------------------------------------------------------------------------------------
// Next Image Manipulator
//----------------------------------------------------------------------------------------------------------------------
//
// Command line:
//
//      nim <command> <command paramters> <options>
//
// Commands:
//
//      palette <filename.pal>              Generate a .nip file (Next Image Palette)
//      palette -d <filename.nip>           Generate a RRRGGGBB palette as a .nip file
//
//      Shared flags:
//          -9                          Use 9-bit palettes
//          --transparent <colour>      Set transparency colour
//
//      image <filename.ext>                Generate a .nim file.  Supports many image formats.
//          --pal <filename.nip/pal>            Define the palette to use in conversion (otherwise uses default palette)
//
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// F I L E   F O R M A T S
//----------------------------------------------------------------------------------------------------------------------
// N I P   F I L E   F O R M A T
//----------------------------------------------------------------------------------------------------------------------
//  Offset  Length  Description
//  0       4       "NIP0" - tag identifying file format and version
//  4       1       Number of colours (0 == 256)
//  5       1       Flags (bit 0: 0=8-bit, 1=9-bit)
//  6       N       Palette data
//  6+N     1       Transparency colour
//
// The palette data will comprise of RRRGGGBB data * number of colours, but if bit 0 of the flags is set to 1, then
// another block of 0000000B data * number of colours will be interleaved with the first block.  One byte from one,
// followed by another byte from the second.
//
//----------------------------------------------------------------------------------------------------------------------
// N I M   F I L E   F O R M A T
//----------------------------------------------------------------------------------------------------------------------
//  Offset  Length  Description
//  0       4       "NIM0" - tag identifying file format and version
//  4       2       Width (little endian)
//  6       2       Height (little endian)
//  8       N       Pixel data (0-255)
//
// N = 1 byte * Width * Height or 0.5 byte * width * height
//
//----------------------------------------------------------------------------------------------------------------------


#include <core.h>
#include <cassert>
#include <cmdline.h>
#include <iostream>
#include <fstream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace fs = std::filesystem;

//----------------------------------------------------------------------------------------------------------------------
// Utilties
//----------------------------------------------------------------------------------------------------------------------

static int kColour_3bit[] = { 0, 36, 73, 109, 146, 182, 219, 255 };
static int kColour_2bit[] = { 0, 85, 170, 255 };

func gfxReduce3(u8 v) -> u8
{
    int minIdx = 0;
    int minDiff = 255;

    for (int i = 0; i < 8; ++i)
    {
        int diff = abs((int)v - (int)kColour_3bit[i]);
        if (0 == diff)
        {
            minIdx = i;
            break;
        }
        if (diff < minDiff)
        {
            minDiff = diff;
            minIdx = i;
        }
    }

    return (u8)minIdx;
}

func gfxReduce2(u8 v) -> u8
{
    int minIdx = 0;
    int minDiff = 255;

    for (int i = 0; i < 4; ++i)
    {
        int diff = abs((int)v - (int)kColour_2bit[i]);
        if (0 == diff)
        {
            minIdx = i;
            break;
        }
        if (diff < minDiff)
        {
            minDiff = diff;
            minIdx = i;
        }
    }

    return (u8)minIdx;
}


//----------------------------------------------------------------------------------------------------------------------
// Data structures
//----------------------------------------------------------------------------------------------------------------------

struct Colour
{
    Colour(u8 red, u8 green, u8 blue) : m_red(red), m_green(green), m_blue(blue) {}
    Colour() : Colour(0, 0, 0) {}

    u8 m_red;
    u8 m_green;
    u8 m_blue;
};

class Palette
{
public:
    Palette()
        : m_transparentColour(0xe3)
    {
        for (int i = 0; i < 256; ++i)
        {
            u8 r = (i & 0xe0) >> 5;
            u8 g = (i & 0x1c) >> 2;
            u8 b = ((i & 0x03) << 1) + (((i & 0x02) >> 1) | (i & 0x01));

            m_colours.push_back(Colour(r, g, b));
        }
    }

    Palette(ifstream& f)
        : m_transparentColour(0xe3)
    {
        vector<Colour> colours;

        u32 tag;
        f.read((char *)&tag, 4);
        if (tag == '0PIN')
        {
            // .NIP file
            u8 numColours;
            u8 flags;

            f.read((char *)&numColours, 1);
            f.read((char *)&flags, 1);

            int actualNumColours = numColours ? numColours : 256;
            for (int i = 0; i < actualNumColours; ++i)
            {
                u8 p1, p2;
                f.read((char *)&p1, 1);
                if (flags & 1)
                {
                    f.read((char *)&p2, 1);
                }
                else
                {
                    p2 = ((p1 & 2) >> 1) | (p1 & 1);
                }

                u8 r = ((p1 & 0xe0) >> 5);
                u8 g = ((p1 & 0x1c) >> 2);
                u8 b = ((p1 & 0x03) << 1) | (p2 & 1);

                colours.push_back(Colour(r, g, b));
            }

            f.read((char *)&m_transparentColour, 1);
        }
        else
        {
            f.seekg(0, ios::beg);

            string line;
            f >> line;
            if (line == "JASC-PAL")
            {
                f >> line;
                if (line == "0100")
                {
                    f >> line;
                    size_t num = size_t(stoi(line));

                    int i = 0;
                    while (!f.eof())
                    {
                        int red, green, blue;
                        f >> red >> green >> blue;

                        if (red < 0 || red > 255 ||
                            green < 0 || green > 255 ||
                            blue < 0 || blue > 255)
                        {
                            cerr << "ERROR: Invalid .pal file" << endl;
                            return;
                        }

                        colours.emplace_back(gfxReduce3(u8(red)), gfxReduce3(u8(green)), gfxReduce3(u8(blue)));

                        if (++i == num) break;
                    }

                    if (num != colours.size())
                    {
                        cerr << "ERROR: Invalid number of colours found in the palette." << endl;
  
                        //#todo: truncate or expand with black rather than error?
                        return;
                    }
                }
            }
        }


        m_colours = move(colours);
    }

    func numColours() const -> int{ return int(m_colours.size()); }

    func operator[] (int i) const -> const Colour& { return m_colours[i]; }

    func getTransColour() const -> u8 { return m_transparentColour; }

    func write(ofstream& f, bool extended) const -> bool
    {
        struct Header
        {
            char id[4];
            u8 numColours;
            u8 flags;
        };

        Header h;
        h.id[0] = 'N';
        h.id[1] = 'I';
        h.id[2] = 'P';
        h.id[3] = '0';
        h.numColours = u8(m_colours.size() & 0xff);
        h.flags = extended ? 1 : 0;

        f.write((char *)&h, sizeof(h));

        for (const auto& colour : m_colours)
        {
            u8 p1 = ((colour.m_red) << 5) | ((colour.m_green) << 2) | ((colour.m_blue >> 1));
            u8 p2 = (colour.m_blue & 1);

            f.write((char *)&p1, 1);
            if (extended) f.write((char *)&p2, 1);
        }

        f.write((char *)&m_transparentColour, 1);

        return 0;
    }

    func setTransparent(u8 index) -> void
    {
        m_transparentColour = index;
    }

private:
    vector<Colour> m_colours;
    u8 m_transparentColour;
};

//----------------------------------------------------------------------------------------------------------------------
// Palette command handler
//----------------------------------------------------------------------------------------------------------------------

func write_palette(const Palette& p, fs::path outPath, bool extended) -> int
{
    ofstream f(outPath, ios::binary | ios::trunc);
    if (!f.is_open())
    {
        cerr << "ERROR: Unable to create file " << outPath << endl;
        return 1;
    }

    p.write(f, extended);
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

func indexStr(const string& str) -> u8
{
    if (str[0] == '$')
    {
        // hex string
        u8 t = 0;
        for (auto c : str)
        {
            if (c >= '0' && c <= '9') { t <<= 4; t += (c - '0'); }
            if (c >= 'a' && c <= 'f') { t <<= 4; t += (c - 'a' + 10); }
            if (c >= 'A' && c <= 'F') { t <<= 4; t += (c - 'A' + 10); }
        }

        return t;
    }
    else
    {
        return u8(stoi(str));
    }
}

//----------------------------------------------------------------------------------------------------------------------

func palette_handler(const CmdLine& cmdLine) -> int
{
    if (cmdLine.numParams() != 1)
    {
        cerr << "ERROR: Invalid parameters." << endl;
        cerr << "Syntax: " << endl
            << "    nim palette <filename.pal>  - Generate .nip file." << endl
            << "    nim palette --default       - Generate default RRRGGGBB palette." << endl;
        return 1;
    }

    fs::path outPath = cmdLine.param(0);
    outPath.replace_extension(".nip");
    bool extended = cmdLine.flag('9');

    auto transparentColourString = cmdLine.longFlag("transparent");
    u8 transColour = 0xe3;
    if (!transparentColourString.empty())
    {
        transColour = indexStr(transparentColourString);
    }

    if (cmdLine.flag('d'))
    {
        // Create default palette
        Palette p;
        p.setTransparent(transColour);
        return write_palette(p, outPath, extended);
    }
    else
    {
        // Open the input palette file.
        ifstream f(cmdLine.param(0), ios::binary);
        if (!f.is_open())
        {
            cerr << "ERROR: Unable to open file '" << cmdLine.param(0) << "'." << endl;
            return 1;
        }

        // Read it
        Palette p(f);
        p.setTransparent(transColour);
        if (p.numColours() == 0)
        {
            cerr << "ERROR: Unable to load the palette file '" << cmdLine.param(0) << "'." << endl;
            return 1;
        }

        return write_palette(p, outPath, extended);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Image command handler
//----------------------------------------------------------------------------------------------------------------------

func process_image(const Palette& p, const CmdLine& cmdLine) -> int
{
    int w, h, bpp;
    fs::path path = cmdLine.param(0);

    u32* img = (u32 *)stbi_load(path.string().c_str(), &w, &h, &bpp, 4);
    if (!img)
    {
        cerr << "ERROR: Could not load image " << path << endl;
        return 1;
    }
    vector<u8> dst;
    bool bit4 = false;

    if (cmdLine.flag('4'))
    {
        if (p.numColours() > 16)
        {
            cerr << "ERROR: Invalid palette for 4-bit mode.  Must be 16 colours or less." << endl;
            return 1;
        }

        if (w % 2 == 1)
        {
            cerr << "ERROR: Width must be a multiple of 2 for 4-bit mode." << endl;
            return 1;
        }

        bit4 = true;
    }

    // Convert image
    u32* s = img;
    for (int row = 0; row < h; ++row)
    {
        u8 idx = 0;

        for (int col = 0; col < w; ++col)
        {
            u8 a = (*s & 0xff000000) >> 24;
            u8 b = (*s & 0x00ff0000) >> 16;
            u8 g = (*s & 0x0000ff00) >> 8;
            u8 r = (*s & 0x000000ff);

            u8 x;   // Final index value

            if (a != 255)
            {
                x = p.getTransColour();
            }
            else
            {
                float p0[3] = { float(r), float(g), float(b) };
                float d = 256 * 256 * 256;

                for (int i = 0; i < p.numColours(); ++i)
                {
                    if (i == p.getTransColour()) continue;

                    float p1[3] = { float(kColour_3bit[p[i].m_red]), float(kColour_3bit[p[i].m_green]), float(kColour_3bit[p[i].m_blue]) };

                    float dx = p1[0] - p0[0];
                    float dy = p1[1] - p0[1];
                    float dz = p1[2] - p0[2];
                    float dist = (dx*dx + dy * dy + dz * dz);

                    if (dist < d)
                    {
                        d = dist;
                        x = i;
                    }
                }
            }

            if (bit4)
            {
                if (col % 2 == 0)
                {
                    // First nibble
                    idx = (x << 4);
                }
                else
                {
                    idx = idx + x;
                    dst.push_back(idx);
                }
            }
            else
            {
                dst.push_back(x);
            }

            ++s;
        }
    }

    assert((!bit4 && (dst.size() == w * h)) || (bit4 && (dst.size() == (w * h / 2))));

    fs::path outPath = cmdLine.param(0);
    outPath.replace_extension(".nim");
    ofstream f(outPath, ios::binary | ios::trunc);
    if (f)
    {
        struct Header
        {
            char id[4];
            u16 width;
            u16 height;
        };

        Header hdr;
        hdr.id[0] = 'N';
        hdr.id[1] = 'I';
        hdr.id[2] = 'M';
        hdr.id[3] = '0';
        hdr.width = w;
        hdr.height = h;
        f.write((char *)&hdr, sizeof(Header));
        f.write((char *)dst.data(), dst.size());
        f.close();
    }
    else
    {
        cerr << "ERROR: Unable to open " << outPath << endl;
        return 1;
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------

func image_handler(const CmdLine& cmdLine) -> int
{
    if (cmdLine.numParams() != 1)
    {
        cerr << "ERROR: Invalid parameters." << endl;
        cerr << "Syntax: " << endl
            << "    nim image <options> <filename.ext>  - Generate .nim file." << endl
            << endl
            << "Options:" << endl
            << "    --pal <filename.nip/.pal>   - Define palette to use in conversion." << endl;
        return 1;
    }

    auto palStr = cmdLine.longFlag("pal");
    u8 transparentIndex = 0xe3;

    if (!palStr.empty())
    {
        ifstream f(palStr, ios::binary);
        if (f.is_open())
        {
            Palette p(f);
            return process_image(p, cmdLine);
        }
        else
        {
            cerr << "ERROR: Cannot file '" << palStr << "'." << endl;
            return 1;
        }
    }
    else
    {
        Palette p;
        return process_image(p, cmdLine);
    }


    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Main entry point
//----------------------------------------------------------------------------------------------------------------------

func main(int argc, char** argv) -> int
{
    CmdLine cmdLine(argc, argv);

    cmdLine.addCommand("palette", palette_handler);
    cmdLine.addCommand("image", image_handler);

    if (cmdLine.dispatch() == -1)
    {
        cerr << "ERROR: Unknown command." << endl << endl;

        cout << "Syntax: nim <command> <command-params>" << endl
            << "Commands:" << endl
            << "    palette <flags> <filename.pal>     Generate a .nip file" << endl
            << "    palette <flags> -d <filename.nip>  Generate a default RRRGGGBB palette" << endl
            << "    image <flags> <filename.ext>       Generate a .nim file from source image" << endl << endl
            << "palette flags:" << endl
            << "    -9                                 Use 9-bit palettes (RRRGGGBBB)" << endl
            << "    --transparent <colour>             Set transparency colour (index only)" << endl
            << "image flags:" << endl
            << "    --pal <filename.nip/pal>           Define the palette to use in conversion" << endl
            << "    -4                                 Output 4-bit graphics" << endl
            << endl;
    }
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
