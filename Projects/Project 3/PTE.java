// page table is essentially an array containing the address that maps to physical memory
public class PTE {
  private long frame;          // the page number in physical memory
  private int dirtyBit;       // 1 = data on page modified, else 0
  private int refBit;         // 1 = page is accessed, else 0

  // constructor for PTE
  public PTE() {
    frame = 0;
    dirtyBit = 0;
    refBit = 0;
  }

  public PTE(long frame, int dirtyBit, int refBit) {
    this.frame = frame;
    this.dirtyBit = dirtyBit;
    this.refBit = refBit;
  }

  // GETTERS: frame, dirtyBit, refBit
  public long getFrame() {
    return frame;
  }

  public int getDirty() {
    return dirtyBit;
  }

  public int getRef() {
    return refBit;
  }

  // SETTERS: frame, dirtyBit, refBit, validBit
  public void setFrame(long frame) {
    this.frame = frame;
  }

  public void setDirty(int dirtyBit) {
    this.dirtyBit = dirtyBit;
  }

  public void setRef(int refBit) {
    this.refBit = refBit;
  }


} // end PTE
