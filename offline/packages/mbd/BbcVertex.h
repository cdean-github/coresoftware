#ifndef G4BBC_BBCVERTEX_H
#define G4BBC_BBCVERTEX_H

#include <phool/PHObject.h>

#include <iostream>
#include <limits>

class BbcVertex : public PHObject
{
 public:
  ~BbcVertex() override = default;

  // PHObject virtual overloads

  void identify(std::ostream& os = std::cout) const override { os << "BbcVertex base class" << std::endl; }
  PHObject* CloneMe() const override { return nullptr; }
  int isValid() const override { return 0; }

  // vertex info

  virtual unsigned int get_id() const { return 0xFFFFFFFF; }
  virtual void set_id(unsigned int) {}

  virtual float get_t() const { return std::numeric_limits<float>::quiet_NaN(); }
  virtual void set_t(float) {}

  virtual float get_t_err() const { return std::numeric_limits<float>::quiet_NaN(); }
  virtual void set_t_err(float) {}

  virtual float get_z() const { return std::numeric_limits<float>::quiet_NaN(); }
  virtual void set_z(float) {}

  virtual float get_z_err() const { return std::numeric_limits<float>::quiet_NaN(); }
  virtual void set_z_err(float) {}

  virtual void set_bbc_ns(int, int, float, float) {}
  virtual int get_bbc_npmt(int) const { return std::numeric_limits<int>::max(); }
  virtual float get_bbc_q(int) const { return std::numeric_limits<float>::quiet_NaN(); }
  virtual float get_bbc_t(int) const { return std::numeric_limits<float>::quiet_NaN(); }

 protected:
  BbcVertex() {}

 private:
  ClassDefOverride(BbcVertex, 1);
};

#endif
