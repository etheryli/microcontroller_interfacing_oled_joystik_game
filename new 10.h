#include <iostream>
#include <cstdlib.h>


int main() {
	difficulty_selection = 1;
	switch(difficulty_selection)
                    {
                        case 1:
                            traps = 1;
                            prizes = 1;
                            break;
                        case 2:
                            traps = 2;
                            prizes = 2;
                            break;
                        case 3:
                            traps = 4;
                            prizes = 4;
                            break;
                    }

					// Pick the x and y randomly
                    for (int i = 0; i < traps; i++)
                    {
						// 41 for easy, so 0 to 40, then 8 to 48
						tcol = rand() % 2; //randomly chooses column
                        traps_x[i] = (rand() % ((104 / (traps+prizes)) + 1)) + 8;
                        traps_y[i] = rand() % 25;
                        prizes_x[i] = (rand() % ((104 / (traps+prizes)) - 1)) + 8;
                        prizes_y[i] = rand() % 25;
						// Chooses column 1 or 2, and add
						if (tcol)
							traps_x[i] += 104 / (traps+prizes);
						else
							prizes_x[i] += 104 / (traps+prizes);
                    }
					
					// Account for columns
					// Draw the traps and prizes
					for (int i = 0; i < traps; i++)
					{
						traps_x[i] = traps_x[i] + i*(104/traps);
						prizes_x[i] = prizes_x[i] + i*(104/traps);
						
					}	