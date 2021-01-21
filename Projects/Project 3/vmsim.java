import java.util.*;
import java.util.Hashtable;
import java.io.*;
import java.math.*;

// ./vmsim –n <numFrames> -p <pageSize in KB> -s <memory split> <traceFile>
public class vmsim{
    static int proc0Frames = 0;
    static int proc1Frames = 0;

  public static void main(String[] args) throws IOException{
    int numFrames = 0;
    int pageSize = 0;
    int memSplit1 = 0;
    int memSplit2 = 0;
    String traceFile = "";

    // getting the command line arguments
    if(args.length != 7) {
      System.out.println("Please enter in the format: java vmsim –n <numframes> -p <pagesize in KB> -s <memory split> <tracefile>");
      System.exit(0);
    }
    else {
      if(args[0].equals("-n") && args[2].equals("-p")) {
        // obtaining the numFrames and page sizes given by user
        numFrames = Integer.parseInt(args[1]);
        pageSize = Integer.parseInt(args[3]);
      }
      if(args[4].equals("-s")) {
        // suppose the ratio was 1:2
        memSplit1 = Integer.parseInt(String.valueOf(args[5].charAt(0)));  // memSplit1 = 1
        memSplit2 = Integer.parseInt(String.valueOf(args[5].charAt(2)));  // memSplit = 2
      }
      traceFile = args[6];
    }

    // Splitting Frames Between 2 Processes: (total frames / ratio) * (p0 or p1) = frames for p0 and fraems for p1
    proc0Frames = (numFrames/(memSplit1 + memSplit2)) * memSplit1;
    proc1Frames = (numFrames/(memSplit1 + memSplit2)) * memSplit2;


    secondChance(numFrames, proc0Frames, proc1Frames, pageSize, traceFile); // might not evn need numframes for this
  } // end main

  // NOTE TO SELF: visit lecture 11/02/2020, 56:40 to get idea of how second chance works
  public static void secondChance(int totalFrames, int proc0Frames, int proc1Frames, int pSize, String trace) throws IOException{
    BufferedReader br;
    int totalMemAccess = 0; 
    int pageFault = 0;          // increment when it does not exist currently in physical memory (RAM)
    int diskWrite = 0;          // increment if a page was modified

    int pageFault1 = 0;         // pagefaults for proc 1
    int diskWrite1 = 0;         // diskwrites for proc1

    int proc0MemAccess = 0;
    int proc1MemAccess = 0;
    int offSetBits = 0;

    LinkedList<PTE> framesList0 = new LinkedList<PTE>();
    LinkedList<PTE> framesList1 = new LinkedList<PTE>();
    // ensure that the user had entered a valid file
    try {
      br = new BufferedReader(new FileReader(trace));
      
      String readBr = br.readLine();
      while(readBr != null) {
        totalMemAccess += 1;                              // total memory access = the number of lines in the .trace files (may delete... depending on gradescope's expected outputs)

        // Splitting the line from tracefil
        String[] splitLine = readBr.split("\\s+");
        char accessType = splitLine[0].charAt(0);         // l = load, s = store
        String address = splitLine[1];                    // 0x00000000
        int pMemAccess = Integer.parseInt(splitLine[2]);  // 0 or 1 = proc0 or proc1

        //  4 KB = 4096 bytes, 1KB = 1024 bytes
        // Convert Hex 2 Bin: find how many bits you need to represent the page, number of bits for page = whatever is left
        String conversion = convertHex2Bin(address.substring(2));                           // converting address Hex to Binary
        long offSetDec = Long.parseLong(conversion.substring(conversion.length()-12), 2);   // getting the address's offset in decimal
        long convert2KB = pSize * 1024;                                                     // converting pageSize*1024 bytes to KB
        offSetBits = (int) (Math.log(convert2KB) / Math.log(2));                            // obtaining the least-significant bits
        //pageSize = (int) Math.pow(2, offSetBits);                                         // pageSize = 2^d where d = the offset bits
        long baseAddr = Long.parseLong(address.substring(2), 16) >> offSetBits;             // getting the base address in decimal; shifting right to get the page number (the base address of page in physical memory)
        long pageTotal = Long.parseLong(address.substring(2), 16);                          // this is the value of the entire hex value
        long offset = Long.parseLong(conversion.substring(conversion.length()-offSetBits), 2); // this gets the 12-bits (for 30 frames, 4KB page siz) needed for offset 

        System.out.println("OFF: " + offSetBits);

        // get the process memory access by P0 and P1
        if(pMemAccess == 0) {
          proc0MemAccess++;  

          PTE pte = new PTE();            // create a new pte and set it's frame to the physAddr we calculate
          pte.setFrame(baseAddr);

          // if the list is empty right now, let's add the first element
          if(framesList0.isEmpty()) {
            if(accessType == 's') {
              pte.setDirty(1);
            }
            framesList0.add(pte);
            pageFault += 1;
          }
          // framesList0 isn't empty
          else {
            if(framesList0.size() < proc0Frames) {

              boolean match = false;
              // loop through the list to see if it has a matching frame
              for(int i = 0 ; i < framesList0.size(); i++) {
                if(framesList0.get(i).getFrame() == pte.getFrame()) {

                  // if it does, set its dirty bit if it's dirty
                  if(accessType == 's') {
                    framesList0.get(i).setDirty(1);
                  }

                  match = true;
                  break; // break out of the loop because we found a match
                }
              }
              if(match == false) {
                if(accessType == 's') {
                  pte.setDirty(1);
                }
                framesList0.add(pte);
                pageFault += 1;
              }
            } // if framesList0.size() < proc0Frames
            else { // framesList0.size() >= proc0Frames
              // decide who to evict
              // first, loop through the list to see if it exists in the linkedlist
              boolean match = false;
              for(int i = 0; i < framesList0.size(); i++) {
                if(framesList0.get(i).getFrame() == pte.getFrame()) {

                  // if it does, set its dirty bit if it's dirty
                  if(accessType == 's') {
                    framesList0.get(i).setDirty(1);
                  }
                  framesList0.get(i).setRef(1);

                  match = true;
                  break; // break out of the loop because we found a match
                }
              }// end for loop to find a matching frame

              // if we didn't find a match during the for loop, let's start traversing through linkedlist
              if(match == false) {

                // if the oldest is a 0
                if(framesList0.getFirst().getRef() == 0) {
                  // if it was dirty, increment diskwrite
                  if(framesList0.getFirst().getDirty() == 1) {
                    diskWrite += 1;
                  }
                  framesList0.remove();  // delete it

                  // store the pte at end of the list
                  if(accessType == 's') {
                    pte.setDirty(1);
                  }
                  framesList0.add(pte);
                  pageFault += 1;

                }
                // if the oldest is currently a 1
                else if(framesList0.getFirst().getRef() == 1) {

                  // give it a second chacne
                  while(framesList0.getFirst().getRef() == 1) {
                    //reset the refBit, add it to the tail, and remove it so it's not the oldest
                    framesList0.getFirst().setRef(0);
                    PTE addToTail = framesList0.getFirst();
                    framesList0.add(addToTail);
                    framesList0.remove();

                    // if we find a ref bit that's a 0, this is the one we want to evict
                    if(framesList0.getFirst().getRef() == 0) {
                      if(framesList0.getFirst().getDirty() == 1) {
                        diskWrite += 1;
                      }
                      framesList0.remove();
                      if(accessType == 's') {
                        pte.setDirty(1);
                      }
                      framesList0.add(pte);
                      pageFault += 1;
                      break;
                    } // end in the while loop if there was a ref we found that's 0
                  }// end while framesList0.getFirst().getRef() == 1
                } // end framesList0.getFirst().getRef() == 1
              } // end if match == false
            } // end else framesList0.size() >= proc0Frames
          } // end "else framesList0 isn't empty"
        } // end if pMemAcess == 0
        else{
          proc1MemAccess++;

          PTE pte = new PTE();           
          pte.setFrame(baseAddr);

          // if the list is empty right now, let's add the first element
          if(framesList1.isEmpty()) {
            if(accessType == 's') {
              pte.setDirty(1);
            }
            framesList1.add(pte);
            pageFault1 += 1;
          }
          // framesList0 isn't empty
          else {
            if(framesList1.size() < proc1Frames) {

              boolean match = false;
              // loop through the list to see if it has a matching frame
              for(int i = 0 ; i < framesList1.size(); i++) {
                if(framesList1.get(i).getFrame() == pte.getFrame()) {

                  // if it does, set its dirty bit if it's dirty
                  if(accessType == 's') {
                    framesList1.get(i).setDirty(1);
                  }

                  match = true;
                  break; // break out of the loop because we found a match
                }
              }
              if(match == false) {
                if(accessType == 's') {
                  pte.setDirty(1);
                }
                framesList1.add(pte);
                pageFault1 += 1;
              }
            } // if framesList0.size() < proc0Frames
            else { // framesList0.size() >= proc0Frames
               // decide who to evict
              // first, loop through the list to see if it exists in the linkedlist
              boolean match = false;
              for(int i = 0; i < framesList1.size(); i++) {
                if(framesList1.get(i).getFrame() == pte.getFrame()) {

                  // if it does, set its dirty bit if it's dirty
                  if(accessType == 's') {
                    framesList1.get(i).setDirty(1);
                  }
                  framesList1.get(i).setRef(1);

                  match = true;
                  break; // break out of the loop because we found a match
                }
              }// end for loop to find a matching frame

              // if we didn't find a match during the for loop, let's start traversing through linkedlist
              if(match == false) {

                // if the oldest is a 0
                if(framesList1.getFirst().getRef() == 0) {
                  // if it was dirty, increment diskwrite
                  if(framesList1.getFirst().getDirty() == 1) {
                    diskWrite1 += 1;
                  }
                  framesList1.remove();  // delete it

                  // store the pte at end of the list
                  if(accessType == 's') {
                    pte.setDirty(1);
                  }
                  framesList1.add(pte);
                  pageFault1 += 1;

                }
                // if the oldest is currently a 1
                else if(framesList1.getFirst().getRef() == 1) {

                  // give it a second chacne
                  while(framesList1.getFirst().getRef() == 1) {
                    //reset the refBit, add it to the tail, and remove it so it's not the oldest
                    framesList1.getFirst().setRef(0);
                    PTE addToTail = framesList1.getFirst();
                    framesList1.add(addToTail);
                    framesList1.remove();

                    // if we find a ref bit that's a 0, this is the one we want to evict
                    if(framesList1.getFirst().getRef() == 0) {
                      if(framesList1.getFirst().getDirty() == 1) {
                        diskWrite1 += 1;
                      }
                      framesList1.remove();
                      if(accessType == 's') {
                        pte.setDirty(1);
                      }
                      framesList1.add(pte);
                      pageFault1 += 1;
                      break;
                    } // end in the while loop if there was a ref we found that's 0
                  }// end while framesList1.getFirst().getRef() == 1
                } // end framesList1.getFirst().getRef() == 1
              } // end if match == false
            } // end else framesList1.size() >= proc0Frames

          } // end "else framesList1 isn't empty"
          
        }

        readBr = br.readLine(); // read the next line
      } // end while loop
    } catch(FileNotFoundException fnfe) {
      System.out.println("'" + trace + "' not found. Please enter a valid filename.");
      System.exit(0);
    }

    // display the final obtained results
    outputProc(0, proc0Frames, pSize, proc0MemAccess, pageFault, diskWrite);   // testing splits for guidance
    outputProc(1, proc1Frames, pSize, proc1MemAccess, pageFault1, diskWrite1); 
    // output(totalFrames, pSize, totalMemAccess, (pageFault + pageFault1), (diskWrite + diskWrite1));
  }

  // conversion method and keeping 32-bit format
  private static String convertHex2Bin(String hex) {
    String conversion = Long.toBinaryString(Long.parseLong(hex.substring(2), 16));

    // leading 0's to maintain 32-bit format... i'm most likely doing this wrong so come back to it once i figure out :(
    while(conversion.length() < 32) {
      conversion = "0" + conversion;
    }
    return conversion;
  }

  // // this is for testing purposes (based off of TA's given format)
  private static void outputProc(int procNum, int frames, int pSize, int memAccess, int pFaults, int writes) {
    System.out.println("For process " + procNum + ".");
    System.out.println("Number of frames: " + frames);
    System.out.println("Total memory accesses: " + memAccess);
    System.out.println("Total page faults: " + pFaults);
    System.out.println("Total writes to disk: " + writes);
    System.out.println();
  }

  // writing the output for Second Chance
  // private static void output(int frames, int pSize, int memAccess, int pFaults, int writes) {
  //   System.out.println("Algorithm: Second Chance");
  //   System.out.println("Number of frames: " + frames);
  //   System.out.printf("Page size: %d KB\n", pSize);
  //   System.out.println("Total memory accesses: " + memAccess);
  //   System.out.println("Total page faults: " + pFaults);
  //   System.out.println("Total writes to disk: " + writes);
  // }

} // end vmsim
