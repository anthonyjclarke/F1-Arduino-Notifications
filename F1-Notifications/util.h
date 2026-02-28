// ----------------------------
// util.h
// Utility functions — race name abbreviations for display output.
// ----------------------------

const char *convertRaceName(const char *raceName)
{
    if (strcmp(raceName, "Emilia Romagna Grand Prix") == 0)
    {
        return "Imola";
    }
    else
    {
        return raceName;
    }
}